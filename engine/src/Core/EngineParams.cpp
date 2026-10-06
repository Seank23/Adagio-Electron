#include "EngineParams.h"

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

		// True when args has no such key. Otherwise it must be an integer from min to max.
		bool ReadRange(const nlohmann::json& args, const char* key, int min, int max, int& outValue, bool& outPresent, std::string& outError)
		{
			outPresent = args.contains(key);
			if (!outPresent)
				return true;

			const nlohmann::json& value = args.at(key);
			if (value.is_number_integer())
			{
				const int64_t number = value.get<int64_t>();
				if (number >= min && number <= max)
				{
					outValue = (int)number;
					return true;
				}
			}

			outError = std::string(key) + " is an integer from " + std::to_string(min) + " to " + std::to_string(max) + ".";
			return false;
		}
	}

	bool UpdateEngineParams(EngineParams& params, const nlohmann::json& args, std::string& outError)
	{
		if (!args.is_object())
		{
			outError = "setEngineParams needs an object.";
			return false;
		}

		int sampleRate = 0;
		int frameLength = 0;
		int hopSize = 0;
		int frameSmoothing = 0;
		bool hasSampleRate = false;
		bool hasFrameLength = false;
		bool hasHopSize = false;
		bool hasFrameSmoothing = false;
		if (!ReadChoice(args, "sampleRate", Protocol::SampleRates, sampleRate, hasSampleRate, outError)
			|| !ReadChoice(args, "frameLength", Protocol::FrameLengths, frameLength, hasFrameLength, outError)
			|| !ReadChoice(args, "hopSize", Protocol::HopSizes, hopSize, hasHopSize, outError)
			|| !ReadRange(args, "frameSmoothing", Protocol::FrameSmoothingMin, Protocol::FrameSmoothingMax, frameSmoothing, hasFrameSmoothing, outError))
			return false;

		if (!hasSampleRate && !hasFrameLength && !hasHopSize && !hasFrameSmoothing)
		{
			outError = "setEngineParams needs a sampleRate, a frameLength, a hopSize or a frameSmoothing.";
			return false;
		}

		if (hasSampleRate)
			params.Analysis.SampleRate = (float)sampleRate;
		if (hasFrameLength)
			params.Analysis.FrameLength = frameLength;
		if (hasHopSize)
			params.Analysis.HopSize = hopSize;
		if (hasFrameSmoothing)
			params.Analysis.FrameSmoothing = frameSmoothing;
		return true;
	}

	nlohmann::json EngineParamsJson(const EngineParams& params)
	{
		return {
			{"sampleRate", (int)params.Analysis.SampleRate},
			{"frameLength", params.Analysis.FrameLength},
			{"hopSize", params.Analysis.HopSize},
			{"frameSmoothing", params.Analysis.FrameSmoothing},
			{"debug", params.Debug}
		};
	}
}
