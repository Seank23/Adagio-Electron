#pragma once
#include <memory>
#include <vector>
#include <unordered_map>

namespace Adagio
{
	struct WaveformPeak
	{
		float Min;
		float Max;
	};

	class AudioData;

	class WaveformBuilder
	{
	public:
		WaveformBuilder(std::vector<int> resolutionList = { 512, 1024, 2048, 4096, 8192 });
		~WaveformBuilder();

		void BuildWaveform(std::shared_ptr<AudioData> audioData);
		const std::vector<WaveformPeak>& GetWaveformData(int resolution) const;
		const std::vector<int>& GetAvailableResolutions() const { return m_Resolutions; }

	private:
		std::unordered_map<int, std::vector<WaveformPeak>> m_WaveformData;
		std::vector<int> m_Resolutions;
	};
}