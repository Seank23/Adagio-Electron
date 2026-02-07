#pragma once
#include <kfr/base.hpp>

namespace Adagio
{
	struct AudioFrame
	{
		uint64_t StartSample = 0;
		uint32_t SampleRate = 0;
		kfr::univector<float> Samples;
	};
}
