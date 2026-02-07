#include "AnalysisPipeline.h"
#include "AnalysisStage.h"

#include <chrono>

namespace Adagio
{
	AnalysisPipeline::AnalysisPipeline()
	{
	}

	AnalysisPipeline::~AnalysisPipeline()
	{
	}

	void AnalysisPipeline::AddStage(std::unique_ptr<AnalysisStage> stage)
	{
		m_Stages.push_back(std::move(stage));
	}

	std::unique_ptr<AnalysisResult> AnalysisPipeline::ProcessFrame(const AudioFrame& frame)
	{
		auto startTime = std::chrono::high_resolution_clock::now();
		AnalysisContext context{ frame };
		context.Windowed = frame.Samples;
		for (const auto& stage : m_Stages)
		{
			stage->Execute(&context);
		}
		std::unique_ptr<AnalysisResult> result = std::make_unique<AnalysisResult>();
		result->Magnitudes = std::vector<float>(context.Magnitudes.begin(), context.Magnitudes.end());
		result->SampleRate = static_cast<float>(frame.SampleRate);
		auto endTime = std::chrono::high_resolution_clock::now();
		result->ExecutionTimeMs = std::chrono::duration<float, std::milli>(endTime - startTime).count();
		return std::move(result);
	}

	nlohmann::json AnalysisPipeline::GetResultJson(const AnalysisResult& result)
	{
		return
		{
			{"type", "analysis"},
			{"value", {
					{"sampleRate", result.SampleRate},
					{"magnitudes", result.Magnitudes},
					{"executionTimeMs", result.ExecutionTimeMs}
				}
			}
		};
	}
}
