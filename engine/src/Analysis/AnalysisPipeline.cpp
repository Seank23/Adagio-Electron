#include "AnalysisPipeline.h"
#include "AnalysisStage.h"
#include "Protocol.generated.h"

#include <chrono>
#include <algorithm>

namespace Adagio
{
	AnalysisPipeline::AnalysisPipeline()
	{
		m_PersistentData = std::make_unique<PersistentData>();
	}

	AnalysisPipeline::~AnalysisPipeline()
	{
	}

	void AnalysisPipeline::AddStage(std::unique_ptr<AnalysisStage> stage)
	{
		stage->Initialise();
		{
			// Holds only what SetSetting has changed; ApplySettings keeps the stage's own default for the rest.
			std::lock_guard<std::mutex> lock(m_SettingsMutex);
			m_Settings[stage->GetName()] = nlohmann::json::object();
		}
		m_Stages.push_back(std::move(stage));
	}

	std::unique_ptr<AnalysisResult> AnalysisPipeline::ProcessFrame(const AudioFrame& frame)
	{
		auto startTime = std::chrono::high_resolution_clock::now();
		std::unique_ptr<AnalysisContext> context = std::make_unique<AnalysisContext>(frame);
		context->PersistentData = m_PersistentData.get();
		context->Samples = frame.Samples;

		bool settingsUpdated = false;
		if (m_CurrentSettingsVersion != m_SettingsVersion.load(std::memory_order_acquire))
		{
			settingsUpdated = true;
			m_CurrentSettingsVersion = m_SettingsVersion.load(std::memory_order_acquire);
		}

		for (const auto& stage : m_Stages)
		{
			if (settingsUpdated)
			{
				std::lock_guard<std::mutex> lock(m_SettingsMutex);
				const nlohmann::json& stageSettings = m_Settings.at(stage->GetName());
				stage->ApplySettings(stageSettings);
			}
			stage->Execute(context.get());
		}
		std::unique_ptr<AnalysisResult> result = std::make_unique<AnalysisResult>();
		result->MaxMagnitude = *std::max_element(context->Magnitudes.begin(), context->Magnitudes.end());
		result->SampleRate = static_cast<float>(frame.SampleRate);
		result->Timestamp = frame.Timestamp;
		auto endTime = std::chrono::high_resolution_clock::now();
		result->ExecutionTimeMs = std::chrono::duration<float, std::milli>(endTime - startTime).count();
		result->Context = std::move(context);
		return std::move(result);
	}

	void AnalysisPipeline::ResetPersistentData()
	{
		*m_PersistentData = PersistentData();
	}

	const AnalysisStage* AnalysisPipeline::FindStage(const std::string& name) const
	{
		for (const auto& stage : m_Stages)
		{
			if (stage->GetName() == name)
				return stage.get();
		}
		return nullptr;
	}

	nlohmann::json AnalysisPipeline::GetSchemaJson() const
	{
		nlohmann::json stages = nlohmann::json::array();

		std::lock_guard<std::mutex> lock(m_SettingsMutex);
		for (const auto& stage : m_Stages)
		{
			const std::string name = stage->GetName();
			const nlohmann::json& values = m_Settings.at(name);

			const nlohmann::json definitions = stage->GetSettingsSchema();

			nlohmann::json settings = nlohmann::json::array();
			for (const auto& definition : definitions.items())
			{
				nlohmann::json entry = definition.value();
				entry["key"] = definition.key();
				entry["value"] = values.value(definition.key(), entry.value("default", nlohmann::json()));
				settings.push_back(std::move(entry));
			}

			stages.push_back({
				{"name", name},
				{"type", stage->GetType() == AnalysisStageType::Processor ? "processor" : "featureExtractor"},
				{"settings", std::move(settings)}
			});
		}

		return { {"version", Protocol::Version}, {"stages", std::move(stages)} };
	}

	bool AnalysisPipeline::SetSetting(const std::string& stage, const std::string& key, const nlohmann::json& value, std::string& outError)
	{
		const AnalysisStage* target = FindStage(stage);
		if (!target)
		{
			outError = "Unknown analysis stage '" + stage + "'.";
			return false;
		}

		const nlohmann::json definitions = target->GetSettingsSchema();
		if (!definitions.contains(key))
		{
			outError = "Stage '" + stage + "' has no setting '" + key + "'.";
			return false;
		}

		const nlohmann::json& definition = definitions.at(key);
		const std::string type = definition.value("type", std::string());
		if (type == "int" && !value.is_number_integer())
		{
			outError = "Setting '" + key + "' is an integer.";
			return false;
		}
		if (type == "float" && !value.is_number())
		{
			outError = "Setting '" + key + "' is a number.";
			return false;
		}
		if (type == "int" || type == "float")
		{
			const float val = value.get<float>();
			const float min = definition.value("min", -std::numeric_limits<float>::infinity());
			const float max = definition.value("max", std::numeric_limits<float>::infinity());
			if (val < min || val > max)
			{
				outError = "Setting '" + key + "' should be between " + std::to_string(min) + " and " + std::to_string(max) + ".";
				return false;
			}
		}
		if (type == "enum")
		{
			const nlohmann::json options = definition.value("options", nlohmann::json::array());
			if (!value.is_string() || std::find(options.begin(), options.end(), value) == options.end())
			{
				outError = "Setting '" + key + "' must be one of " + options.dump() + ".";
				return false;
			}
		}

		std::lock_guard<std::mutex> lock(m_SettingsMutex);
		m_Settings[stage][key] = value;
		m_SettingsVersion++;
		return true;
	}

	nlohmann::json AnalysisPipeline::GetResultJson(const AnalysisResult& result)
	{
		nlohmann::json notesJson;
		for (const auto& note : result.Context->Notes)
			notesJson.push_back({
				{"name", note.Name},
				{"midi", note.Midi},
				{"frequency", note.PeakInfo.Frequency},
				{"magnitude", note.PeakInfo.Magnitude},
				{"score", note.PeakInfo.Score},
				{"errorCents", note.ErrorCents},
				{"timestamp", note.Timestamp}
			});
		nlohmann::json keyHistogramJson;
		for (const auto& pair : result.Context->KeyFrequencyHistogram)
			keyHistogramJson.push_back({ {"frequency", pair.first}, {"score", pair.second} });
		nlohmann::json chordHistogramJson;
		for (const auto& pair : result.Context->ChordFrequencyHistogram)
			chordHistogramJson.push_back({ {"frequency", pair.first}, {"score", pair.second} });
		nlohmann::json chordsJson;
		for (const auto& chord : result.Context->PredictedChords)
			chordsJson.push_back({ {"name", chord.Name}, {"probability", chord.Probability} });

		// The spectrum itself, its maximum and binHz travel in the binary spectrum frame.
		return
		{
			{"type", Protocol::Event::Analysis},
			{"value", {
					{"executionTimeMs", result.ExecutionTimeMs},
					{"sampleRate", result.SampleRate},
					{"notes", notesJson},
					{"keyHistogram", keyHistogramJson},
					{"chordHistogram", chordHistogramJson},
					{"detectedKey", result.Context->DetectedKey},
					{"predictedChords", chordsJson},
				}
			}
		};
	}
}
