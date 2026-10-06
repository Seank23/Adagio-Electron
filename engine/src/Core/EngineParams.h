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
		int FrameSmoothing = Protocol::FrameSmoothingDefault;
	};

	struct EngineParams
	{
		AnalysisParams Analysis;
		bool Debug = false;
	};

	bool UpdateEngineParams(EngineParams& params, const nlohmann::json& args, std::string& outError);

	nlohmann::json EngineParamsJson(const EngineParams& params);
}
