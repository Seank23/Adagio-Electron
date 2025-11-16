#pragma once

#include <kfr/base.hpp>

namespace Adagio
{
	class AudioData
	{
	public:
		AudioData() {}
		AudioData(kfr::univector2d<float> playbackStream)
			: PlaybackStream(playbackStream)
		{

		}

        void Clear()
        {
            PlaybackStream.clear();
            AnalysisStream.clear();
            PlaybackSampleRate = 0.0f;
            AnalysisSampleRate = 0.0f;
            Channels = 0;
            Duration = 0.0f;
        }

		kfr::univector2d<float> PlaybackStream;
		kfr::univector<float> AnalysisStream;

		float PlaybackSampleRate = 0.0f;
		float AnalysisSampleRate = 0.0f;

		int Channels = 0;
		float Duration = 0.0f;
	};
}
