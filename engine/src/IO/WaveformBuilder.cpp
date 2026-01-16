#include "WaveformBuilder.h"
#include "../IO/AudioData.h"

namespace Adagio
{
	WaveformBuilder::WaveformBuilder(std::vector<int> resolutionList)
		: m_Resolutions(resolutionList)
	{
	}

	WaveformBuilder::~WaveformBuilder()
	{
	}

	void WaveformBuilder::BuildWaveform(std::shared_ptr<AudioData> audioData)
	{
		for (int resolution : m_Resolutions)
		{
			std::vector<WaveformPeak> peaks;
			size_t totalFrames = audioData->PCMData[0].size();
			size_t framesPerPeak = resolution;
			size_t numPeaks = (totalFrames + framesPerPeak - 1) / framesPerPeak;
			for (size_t i = 0; i < numPeaks; ++i)
			{
				float maxVal = std::numeric_limits<float>::lowest();
				for (size_t j = 0; j < framesPerPeak; ++j)
				{
					size_t frameIndex = i * framesPerPeak + j;
					if (frameIndex >= totalFrames) break;
					for (int ch = 0; ch < audioData->Channels; ++ch)
					{
						float sample = audioData->PCMData[ch][frameIndex];
						if (sample > maxVal) maxVal = sample;
					}
				}
				peaks.push_back({ maxVal });
			}
			m_WaveformData[resolution] = peaks;
		}
	}

	const std::vector<WaveformPeak>& WaveformBuilder::GetWaveformData(int resolution) const
	{
		static const std::vector<WaveformPeak> empty;
		auto it = m_WaveformData.find(resolution);
		if (it != m_WaveformData.end())
			return it->second;
		return empty;
	}
}