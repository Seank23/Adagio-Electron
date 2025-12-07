#pragma once
#include <kfr/base.hpp>

namespace Adagio
{
	class AudioData
	{
	public:
		AudioData() {}
		AudioData(kfr::univector2d<float> pcmData)
			: PCMData(pcmData)
		{
		}

        void Clear()
        {
			PCMData.clear();
            SampleRate = 0.0f;
            Channels = 0;
			SamplesPerChannel = 0;
            Duration = 0.0f;
        }

		kfr::univector2d<float> PCMData;

		float SampleRate = 0.0f;
		int Channels = 0;
		int SamplesPerChannel = 0;
		float Duration = 0.0f;
	};
}
