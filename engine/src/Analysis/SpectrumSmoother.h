#pragma once
#include <kfr/base.hpp>

#include <span>

namespace Adagio
{
	class SpectrumSmoother
	{
	public:
		void Reset() { m_State.clear(); }

		void Apply(kfr::univector<float>& magnitudes, std::span<const float> binSeconds, int frames, double frameSeconds, double elapsedSeconds);

	private:
		kfr::univector<float> m_State;
	};
}
