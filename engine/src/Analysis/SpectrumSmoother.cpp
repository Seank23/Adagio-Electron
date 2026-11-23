#include "SpectrumSmoother.h"

#include <algorithm>

namespace Adagio
{
	void SpectrumSmoother::Apply(kfr::univector<float>& magnitudes, std::span<const float> binSeconds, int frames, double frameSeconds, double elapsedSeconds)
	{
		if (m_State.size() != magnitudes.size())
		{
			m_State = magnitudes;
			return;
		}

		// An N-frame average lags by (N - 1) / 2 frames, and so does a one-pole filter with that lag, alpha = dt / (dt + lag).
		const double frameLag = std::max(frames - 1, 0) * frameSeconds * 0.5;
		const bool perBin = binSeconds.size() == magnitudes.size();
		for (size_t i = 0; i < magnitudes.size(); i++)
		{
			const double lag = frameLag + (perBin ? 0.5 * std::max(binSeconds[i], 0.0f) : 0.0);
			const float alpha = lag > 0.0 ? (float)(elapsedSeconds / (elapsedSeconds + lag)) : 1.0f;
			m_State[i] += alpha * (magnitudes[i] - m_State[i]);
		}
		magnitudes = m_State;
	}
}
