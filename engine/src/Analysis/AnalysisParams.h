#pragma once
#include "Protocol.generated.h"

#include <nlohmann/json.hpp>

#include <string>

namespace Adagio
{
	struct AnalysisParams
	{
		float SampleRate = (float)Protocol::SampleRateDefault;
		int FrameLength = Protocol::FrameLengthDefault;
		int HopSize = Protocol::HopSizeDefault;
	};

	bool UpdateAnalysisParams(AnalysisParams& params, const nlohmann::json& args, std::string& outError);

	nlohmann::json AnalysisParamsJson(const AnalysisParams& params);
}
