#pragma once

#include "../IO/AudioData.h"
#include "AnalysisProcessor.h"

namespace Adagio
{
	struct AnalysisParams
	{
		float SampleRate = 44100.0f;
		const int FrameLength = 8192;
	};

	class AnalysisService
	{
	public:
		AnalysisService()
			: m_AudioData(nullptr), m_Params(nullptr)
		{

		}
		~AnalysisService();

		int Init(AudioData& audioData, AnalysisParams& params);
		kfr::univector<float> AnalyseFrame(long startSample);

	private:
		kfr::univector<float> GenerateSpectrum(kfr::univector<float> audioFrame);

		AudioData* m_AudioData;
		const AnalysisParams* m_Params;
		std::vector<AnalysisProcessor*> m_Processors;
	};
}

