#include "AnalysisPipeline.h"
#include "AnalysisStage.h"
#include "Protocol.generated.h"
#include "../Util/JsonUtils.h"

#include <chrono>
#include <algorithm>

namespace Adagio
{
	namespace
	{
		// "persistent": false marks a setting that belongs to the loaded track (A4), so it is never saved with the pipeline.
		bool IsNotPersistent(const nlohmann::json& definition)
		{
			const auto persistent = definition.find("persistent");
			return persistent != definition.end() && persistent->is_boolean() && !persistent->get<bool>();
		}
	}

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
		m_Defaults[stage->GetName()] = nlohmann::json::object();
		{
			// Holds only what SetSetting has changed; the rest follow the pipeline's default, then the stage's own.
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
				nlohmann::json merged = m_Defaults.at(stage->GetName());
				{
					std::lock_guard<std::mutex> lock(m_SettingsMutex);
					merged.update(m_Settings.at(stage->GetName()));
				}
				stage->ApplySettings(merged);
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

	nlohmann::json AnalysisPipeline::DefaultsFor(const AnalysisStage& stage) const
	{
		const nlohmann::json& overrides = m_Defaults.at(stage.GetName());

		nlohmann::json defaults = nlohmann::json::object();
		for (const auto& [key, definition] : stage.GetSettingsSchema().items())
			defaults[key] = overrides.value(key, definition.value("default", nlohmann::json()));
		return defaults;
	}

	nlohmann::json AnalysisPipeline::GetSchemaJson() const
	{
		nlohmann::json stages = nlohmann::json::array();

		std::lock_guard<std::mutex> lock(m_SettingsMutex);
		for (const auto& stage : m_Stages)
		{
			const std::string name = stage->GetName();
			const nlohmann::json& values = m_Settings.at(name);
			const nlohmann::json defaults = DefaultsFor(*stage);

			const nlohmann::json definitions = stage->GetSettingsSchema();

			nlohmann::json settings = nlohmann::json::array();
			for (const auto& definition : definitions.items())
			{
				nlohmann::json entry = definition.value();
				entry["key"] = definition.key();
				entry["default"] = defaults.at(definition.key());
				entry["value"] = values.value(definition.key(), entry["default"]);
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

	nlohmann::json AnalysisPipeline::CollectValues(bool perTrack) const
	{
		nlohmann::json stages = nlohmann::json::object();

		std::lock_guard<std::mutex> lock(m_SettingsMutex);
		for (const auto& stage : m_Stages)
		{
			const std::string& name = stage->GetName();
			const nlohmann::json& values = m_Settings.at(name);
			const nlohmann::json defaults = DefaultsFor(*stage);

			nlohmann::json settings = nlohmann::json::object();
			for (const auto& [key, definition] : stage->GetSettingsSchema().items())
			{
				if (IsNotPersistent(definition) != perTrack)
					continue;
				if (perTrack)
					settings[key] = values.value(key, defaults.at(key));
				else if (values.contains(key))
					settings[key] = values.at(key);
			}
			if (!perTrack || !settings.empty())
				stages[name] = std::move(settings);
		}
		return stages;
	}

	nlohmann::json AnalysisPipeline::GetValuesJson() const
	{
		return CollectValues(false);
	}

	nlohmann::json AnalysisPipeline::GetPerTrackValuesJson() const
	{
		return CollectValues(true);
	}

	bool AnalysisPipeline::SetDefault(const std::string& stage, const std::string& key, const nlohmann::json& value, std::string& outError)
	{
		const AnalysisStage* target = FindStage(stage);
		if (!target)
		{
			outError = "Unknown analysis stage '" + stage + "'.";
			return false;
		}

		const nlohmann::json& definitions = target->GetSettingsSchema();
		if (!definitions.contains(key))
		{
			outError = "Stage '" + stage + "' has no setting '" + key + "'.";
			return false;
		}

		const nlohmann::json& definition = definitions.at(key);
		if (IsNotPersistent(definition))
		{
			outError = "Setting '" + key + "' is per track and can't have a pipeline default.";
			return false;
		}
		if (!CheckSetting(definition, key, value, outError))
			return false;

		m_Defaults[stage][key] = value;
		return true;
	}

	bool AnalysisPipeline::SetSetting(const std::string& stage, const std::string& key, const nlohmann::json& value, std::string& outError)
	{
		const AnalysisStage* target = FindStage(stage);
		if (!target)
		{
			outError = "Unknown analysis stage '" + stage + "'.";
			return false;
		}

		const nlohmann::json& definitions = target->GetSettingsSchema();
		if (!definitions.contains(key))
		{
			outError = "Stage '" + stage + "' has no setting '" + key + "'.";
			return false;
		}
		if (!CheckSetting(definitions.at(key), key, value, outError))
			return false;

		const bool isDefault = IsSameValue(value, DefaultsFor(*target).at(key));

		std::lock_guard<std::mutex> lock(m_SettingsMutex);
		if (isDefault)
			m_Settings[stage].erase(key);
		else
			m_Settings[stage][key] = value;
		m_SettingsVersion++;
		return true;
	}

	bool AnalysisPipeline::CheckSetting(const nlohmann::json& definition, const std::string& key, const nlohmann::json& value, std::string& outError)
	{
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
		return true;
	}

	void AnalysisPipeline::ResetSetting(const std::string& stage, const std::string& key)
	{
		// m_Settings holds only what was changed, so dropping the key restores the pipeline's default.
		std::lock_guard<std::mutex> lock(m_SettingsMutex);
		if (!m_Settings.contains(stage) || !m_Settings[stage].contains(key))
			return;
		m_Settings[stage].erase(key);
		m_SettingsVersion++;
	}

	void AnalysisPipeline::ResetAllSettings()
	{
		std::lock_guard<std::mutex> lock(m_SettingsMutex);
		for (const auto& stage : m_Stages)
			m_Settings[stage->GetName()] = nlohmann::json::object();
		m_SettingsVersion++;
	}

	void AnalysisPipeline::ResetPerTrackSettings()
	{
		std::lock_guard<std::mutex> lock(m_SettingsMutex);
		for (const auto& stage : m_Stages)
		{
			for (const auto& [key, definition] : stage->GetSettingsSchema().items())
			{
				if (IsNotPersistent(definition))
					m_Settings[stage->GetName()].erase(key);
			}
		}
		m_SettingsVersion++;
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
		auto& keySig = result.Context->PredictedKey;
		nlohmann::json keyJson = { {"name", keySig.Name}, {"tonic", keySig.Tonic}, {"tonicClass", keySig.TonicClass}, {"mode", keySig.ModeName}, {"scaleClasses", keySig.Scale}};
		const std::optional<float>& tuningError = result.Context->AvgTuningError;
		nlohmann::json tuningErrorJson = tuningError ? nlohmann::json(*tuningError) : nlohmann::json(nullptr);


		// The spectrum itself, its maximum and binHz travel in the binary spectrum frame.
		return
		{
			{"type", Protocol::Event::Analysis},
			{"value", {
					{"executionTimeMs", result.ExecutionTimeMs},
					{"sampleRate", result.SampleRate},
					{"notes", notesJson},
					{"avgTuningError", tuningErrorJson},
					{"keyHistogram", keyHistogramJson},
					{"chordHistogram", chordHistogramJson},
					{"detectedKey", keyJson},
					{"predictedChords", chordsJson},
				}
			}
		};
	}
}
