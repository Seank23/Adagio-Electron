#pragma once

#include <kfr/base.hpp>

namespace Adagio
{
	enum FileFormat
	{
		WAV, MP3, FLAC
	};

	class AudioUtils
	{
	public:
		static void InterleaveStereo(kfr::univector2d<float> stereoIn, std::vector<float>* interleavedOut)
		{
			int sampleCount = stereoIn[0].size();
			for (int i = 0; i < sampleCount; i++)
			{
				interleavedOut->push_back(stereoIn[0][i]);
				interleavedOut->push_back(stereoIn[1][i]);
			}
		}

		static const kfr::univector<float> StereoToMono(kfr::univector2d<float> stereoIn)
		{
			if (stereoIn.size() == 1)
				return stereoIn[0];
			int size = stereoIn[0].size();
			kfr::univector<float> monoOut(size);
			for (int i = 0; i < stereoIn[0].size(); i++)
			{
				float val = (stereoIn[0][i] + stereoIn[1][i]) / 2;
				monoOut[i] = val;
			}
			return monoOut;
		}

		static void UnivectorToMono(kfr::univector<float> univectorIn, std::vector<float>* monoOut)
		{
			int sampleCount = univectorIn.size();
			for (int i = 0; i < sampleCount; i++)
				monoOut->push_back(univectorIn[i]);
		}
	};
}
