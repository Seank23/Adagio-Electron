#include "AnalysisParams.h"

#include <algorithm>
#include <array>

namespace Adagio
{
	namespace
	{
		template<size_t N>
		std::string JoinValues(const std::array<int, N>& values)
		{
			std::string joined;
			for (size_t i = 0; i < N; ++i)
			{
				if (i > 0)
					joined += i + 1 == N ? " or " : ", ";
				joined += std::to_string(values[i]);
			}
			return joined;
		}

		// True when args has no such key. Otherwise it must be an integer from the list.
		template<size_t N>
		bool ReadChoice(const nlohmann::json& args, const char* key, const std::array<int, N>& choices, int& outValue, bool& outPresent, std::string& outError)
		{
			outPresent = args.contains(key);
			if (!outPresent)
				return true;

			const nlohmann::json& value = args.at(key);
			if (value.is_number_integer())
			{
				const int64_t number = value.get<int64_t>();
				if (std::find(choices.begin(), choices.end(), number) != choices.end())
				{
					outValue = (int)number;
					return true;
				}
			}

			outError = std::string(key) + " is one of " + JoinValues(choices) + ".";
			return false;
		}
	}

	bool UpdateAnalysisParams(AnalysisParams& params, const nlohmann::json& args, std::string& outError)
	{
		if (!args.is_object())
		{
			outError = "setEngineParams needs an object.";
			return false;
		}

		int sampleRate = 0;
		int frameLength = 0;
		int hopSize = 0;
		bool hasSampleRate = false;
		bool hasFrameLength = false;
		bool hasHopSize = false;
		if (!ReadChoice(args, "sampleRate", Protocol::SampleRates, sampleRate, hasSampleRate, outError)
			|| !ReadChoice(args, "frameLength", Protocol::FrameLengths, frameLength, hasFrameLength, outError)
			|| !ReadChoice(args, "hopSize", Protocol::HopSizes, hopSize, hasHopSize, outError))
			return false;

		if (!hasSampleRate && !hasFrameLength && !hasHopSize)
		{
			outError = "setEngineParams needs a sampleRate, a frameLength or a hopSize.";
			return false;
		}

		if (hasSampleRate)
			params.SampleRate = (float)sampleRate;
		if (hasFrameLength)
			params.FrameLength = frameLength;
		if (hasHopSize)
			params.HopSize = hopSize;
		return true;
	}

	nlohmann::json AnalysisParamsJson(const AnalysisParams& params)
	{
		return {
			{"sampleRate", (int)params.SampleRate},
			{"frameLength", params.FrameLength},
			{"hopSize", params.HopSize}
		};
	}
}
