#include "AnalysisPipeline.h"
#include "AnalysisStage.h"

#include <chrono>
#include <algorithm>
#include <iostream>

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
		std::string name = stage->GetName();
		m_Settings[name] = {};
		auto settings = stage->GetSettings();
		for (auto& it : settings.items())
			m_Settings[name][it.key()] = it.value()["default"];
		m_Stages.push_back(std::move(stage));
	}

	std::unique_ptr<AnalysisResult> AnalysisPipeline::ProcessFrame(const AudioFrame& frame)
	{
		auto startTime = std::chrono::high_resolution_clock::now();
		std::unique_ptr<AnalysisContext> context = std::make_unique<AnalysisContext>(frame);
		context->PersistentData = m_PersistentData.get();
		context->Samples = frame.Samples;
		for (const auto& stage : m_Stages)
		{
			context->Settings = m_Settings[stage->GetName()];
			stage->Execute(context.get());
		}
		std::unique_ptr<AnalysisResult> result = std::make_unique<AnalysisResult>();
		result->MaxMagnitude = *std::max_element(context->Magnitudes.begin(), context->Magnitudes.end());
		result->SampleRate = static_cast<float>(frame.SampleRate);
		auto endTime = std::chrono::high_resolution_clock::now();
		result->ExecutionTimeMs = std::chrono::duration<float, std::milli>(endTime - startTime).count();
		result->Context = std::move(context);
		return std::move(result);
	}

	nlohmann::json AnalysisPipeline::GetResultJson(const AnalysisResult& result)
	{
		nlohmann::json notesJson;
		for (const auto& note : result.Context->Notes)
			notesJson.push_back({ 
				{"name", note.Name}, 
				{"frequency", note.PeakInfo.Frequency},
				{"magnitude", note.PeakInfo.Magnitude},
				{"score", note.PeakInfo.Score},
				{"errorCents", note.ErrorCents},
				{"timestamp", note.Timestamp}
			});
		nlohmann::json medianJson;
		for (const auto& median : result.Context->LocalMedian)
			medianJson.push_back({ {"bin", median.first}, {"magnitude", median.second} });
		nlohmann::json frequencyHistogramJson;
		for (const auto& pair : result.Context->FrequencyHistogram)
			frequencyHistogramJson.push_back({ {"frequency", pair.first}, {"score", pair.second} });

		return
		{
			{"type", "analysis"},
			{"value", {
					{"executionTimeMs", result.ExecutionTimeMs},
					{"sampleRate", result.SampleRate},
					{"magnitudes", result.Context->Magnitudes},
					{"maxMagnitude", result.MaxMagnitude},
					{"localMedian", medianJson},
					{"notes", notesJson},
					{"frequencyHistogram", frequencyHistogramJson},
					{"detectedKey", result.Context->DetectedKey}
				}
			}
		};
	}
}
