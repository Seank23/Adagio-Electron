#pragma once
#include <cmath>

namespace Adagio
{
	class FrequencyAxis
	{
	public:
		FrequencyAxis() = default;
		FrequencyAxis(float binFreq, float minFreq, int binsPerOctave, bool isLog)
			: m_BinFreq(binFreq), m_MinFreq(minFreq), m_BinsPerOctave(binsPerOctave), m_IsLog(isLog) {}
		virtual ~FrequencyAxis() = default;

		virtual float BinToHz(float bin) const = 0;
		virtual float HzToFractionalBin(float frequency) const = 0;
		virtual float BinWidthAt(float frequency) const = 0;

		int HzToBin(float frequency) const { return (int)std::round(HzToFractionalBin(frequency)); }

		bool IsLog() const { return m_IsLog; }
		void SetIsLog(bool isLog) { m_IsLog = isLog; }

		float BinFreq() const { return m_BinFreq; }
		void SetBinFreq(float binFreq) { m_BinFreq = binFreq; }

		float MinFreq() const { return m_MinFreq; }
		void SetMinFreq(float minFreq) { m_MinFreq = minFreq; }

		int BinsPerOctave() const { return m_BinsPerOctave; }
		void SetBinsPerOctave(int binsPerOctave) { m_BinsPerOctave = binsPerOctave; }

	private:
		bool m_IsLog = false;
		float m_BinFreq = 0.0f;
		float m_MinFreq = 0.0f;
		int m_BinsPerOctave = 0;
	};

	class LogFrequencyAxis : public FrequencyAxis
	{
	public:
		LogFrequencyAxis(float minFreq, int binsPerOctave)
			: FrequencyAxis(0.0f, minFreq, binsPerOctave, true) {}

		float BinToHz(float bin) const override
		{
			return MinFreq() * std::pow(2.0f, bin / (float)BinsPerOctave());
		}
		float HzToFractionalBin(float frequency) const override
		{
			return BinsPerOctave() * std::log2(frequency / MinFreq());
		}
		float BinWidthAt(float frequency) const override
		{
			return frequency * (std::pow(2.0f, 1.0f / (float)BinsPerOctave()) - 1.0f);
		}
	};

	class LinearFrequencyAxis : public FrequencyAxis
	{
	public:
		LinearFrequencyAxis(float binFreq, float minFreq)
			: FrequencyAxis(binFreq, minFreq, 0, false) {}

		float BinToHz(float bin) const override
		{
			return MinFreq() + bin * BinFreq();
		}
		float HzToFractionalBin(float frequency) const override
		{
			return (frequency - MinFreq()) / BinFreq();
		}
		float BinWidthAt(float frequency) const override
		{
			return BinFreq();
		}
	};
}
