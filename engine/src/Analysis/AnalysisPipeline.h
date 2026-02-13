#pragma once
#include <vector>
#include <memory>
#include "AudioFrame.h"

#include <nlohmann/json.hpp>

namespace Adagio
{
	class AnalysisStage;

	struct AnalysisContext
	{
		const AudioFrame& Frame;

		kfr::univector<float> Samples;
		kfr::univector<float> Windowed;
		kfr::univector<kfr::complex<float>> Spectrum;
		kfr::univector<float> Magnitudes;

		nlohmann::json Settings;
	};

	struct AnalysisResult
	{
		std::vector<float> Magnitudes;
		float MaxMagnitude;
		float SampleRate;
		float ExecutionTimeMs;
	};

	class AnalysisPipeline
	{
	public:
		AnalysisPipeline();
		~AnalysisPipeline();

		void AddStage(std::unique_ptr<AnalysisStage> stage);
		std::unique_ptr<AnalysisResult> ProcessFrame(const AudioFrame& frame);

		static nlohmann::json GetResultJson(const AnalysisResult& result);

	private:
		std::vector<std::unique_ptr<AnalysisStage>> m_Stages;
		nlohmann::json m_Settings;
	};
}

