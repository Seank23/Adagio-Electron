// The engine parameters a client or the command line may set, and the preprocessing a
// new analysis sample rate runs the track through again.
#include "../src/Core/EngineParams.h"
#include "../src/Analysis/AnalysisService.h"
#include "../src/IO/AudioData.h"

#include <doctest/doctest.h>

#include <cmath>
#include <string>

namespace
{
	using Adagio::EngineParams;

	constexpr float Pi = 3.14159265358979323846f;

	Adagio::AudioData MakeStereoTone(float hz, float sampleRate, float seconds)
	{
		Adagio::AudioData audio;
		audio.SampleRate = sampleRate;
		audio.Channels = 2;
		audio.SamplesPerChannel = (int)(sampleRate * seconds);
		audio.Duration = seconds;
		audio.PCMData.resize(2);
		for (auto& channel : audio.PCMData)
		{
			channel.resize(audio.SamplesPerChannel);
			for (int i = 0; i < audio.SamplesPerChannel; ++i)
				channel[i] = 0.5f * std::sin(2.0f * Pi * hz * (float)i / sampleRate);
		}
		return audio;
	}

	bool AllFinite(const kfr::univector<float>& samples)
	{
		for (float sample : samples)
		{
			if (!std::isfinite(sample))
				return false;
		}
		return true;
	}
}

TEST_CASE("The defaults come from protocol.json")
{
	const EngineParams params;
	CHECK(params.Analysis.SampleRate == (float)Adagio::Protocol::SampleRateDefault);
	CHECK(params.Analysis.FrameLength == Adagio::Protocol::FrameLengthDefault);
	CHECK(params.Analysis.HopSize == Adagio::Protocol::HopSizeDefault);
	CHECK(params.Analysis.FrameSmoothing == Adagio::Protocol::FrameSmoothingDefault);
	CHECK_FALSE(params.Debug);

	const nlohmann::json json = Adagio::EngineParamsJson(params);
	CHECK(json == nlohmann::json{ {"sampleRate", Adagio::Protocol::SampleRateDefault}, {"frameLength", Adagio::Protocol::FrameLengthDefault}, {"hopSize", Adagio::Protocol::HopSizeDefault}, {"frameSmoothing", Adagio::Protocol::FrameSmoothingDefault}, {"debug", false} });
}

TEST_CASE("Any one value, or several, may be set")
{
	EngineParams params;
	std::string error;

	REQUIRE(Adagio::UpdateEngineParams(params, { {"hopSize", 256} }, error));
	CHECK(params.Analysis.HopSize == 256);
	CHECK(params.Analysis.FrameLength == 4096);

	REQUIRE(Adagio::UpdateEngineParams(params, { {"sampleRate", 16000}, {"frameLength", 8192} }, error));
	CHECK(params.Analysis.SampleRate == 16000.0f);
	CHECK(params.Analysis.FrameLength == 8192);
	CHECK(params.Analysis.HopSize == 256);

	for (int rate : Adagio::Protocol::SampleRates)
		CHECK(Adagio::UpdateEngineParams(params, { {"sampleRate", rate} }, error));
	for (int length : Adagio::Protocol::FrameLengths)
		CHECK(Adagio::UpdateEngineParams(params, { {"frameLength", length} }, error));
	for (int hop : Adagio::Protocol::HopSizes)
		CHECK(Adagio::UpdateEngineParams(params, { {"hopSize", hop} }, error));
}

TEST_CASE("A value off its list is refused with the list")
{
	EngineParams params;
	std::string error;

	CHECK_FALSE(Adagio::UpdateEngineParams(params, { {"hopSize", 100} }, error));
	CHECK(error == "hopSize is one of 32, 64, 128 or 256.");

	CHECK_FALSE(Adagio::UpdateEngineParams(params, { {"sampleRate", 44100} }, error));
	CHECK(error == "sampleRate is one of 4000, 8000 or 16000.");

	CHECK_FALSE(Adagio::UpdateEngineParams(params, { {"frameLength", "4096"} }, error));
	CHECK(error == "frameLength is one of 2048, 4096 or 8192.");

	// 8000.5 is not an integer, and nor, on the wire, is 8000.0.
	CHECK_FALSE(Adagio::UpdateEngineParams(params, { {"sampleRate", 8000.5} }, error));
	CHECK_FALSE(Adagio::UpdateEngineParams(params, { {"sampleRate", 8000.0} }, error));
	CHECK_FALSE(Adagio::UpdateEngineParams(params, { {"hopSize", -64} }, error));
}

TEST_CASE("Frame smoothing takes any count in its range, on its own or with the others")
{
	EngineParams params;
	std::string error;

	REQUIRE(Adagio::UpdateEngineParams(params, { {"frameSmoothing", 7} }, error));
	CHECK(params.Analysis.FrameSmoothing == 7);
	CHECK(params.Analysis.HopSize == Adagio::Protocol::HopSizeDefault);
	CHECK(Adagio::EngineParamsJson(params)["frameSmoothing"] == 7);

	REQUIRE(Adagio::UpdateEngineParams(params, { {"hopSize", 64}, {"frameSmoothing", 2} }, error));
	CHECK(params.Analysis.HopSize == 64);
	CHECK(params.Analysis.FrameSmoothing == 2);

	for (int count = Adagio::Protocol::FrameSmoothingMin; count <= Adagio::Protocol::FrameSmoothingMax; ++count)
	{
		CHECK(Adagio::UpdateEngineParams(params, { {"frameSmoothing", count} }, error));
		CHECK(params.Analysis.FrameSmoothing == count);
	}
}

TEST_CASE("The transport event's analysis carries every analysis value the reply does")
{
	EngineParams params;
	std::string error;
	REQUIRE(Adagio::UpdateEngineParams(params, { {"sampleRate", 16000}, {"hopSize", 32}, {"frameSmoothing", 9} }, error));

	nlohmann::json reply = Adagio::EngineParamsJson(params);
	reply.erase("debug");
	CHECK(Adagio::AnalysisService::ParamsJson(params.Analysis) == reply);
}

TEST_CASE("Frame smoothing outside its range, or not an integer, is refused with the range")
{
	static_assert(Adagio::Protocol::FrameSmoothingMin <= Adagio::Protocol::FrameSmoothingDefault
		&& Adagio::Protocol::FrameSmoothingDefault <= Adagio::Protocol::FrameSmoothingMax);

	EngineParams params;
	std::string error;
	const std::string expected = "frameSmoothing is an integer from " + std::to_string(Adagio::Protocol::FrameSmoothingMin)
		+ " to " + std::to_string(Adagio::Protocol::FrameSmoothingMax) + ".";

	CHECK_FALSE(Adagio::UpdateEngineParams(params, { {"frameSmoothing", Adagio::Protocol::FrameSmoothingMin - 1} }, error));
	CHECK(error == expected);
	CHECK_FALSE(Adagio::UpdateEngineParams(params, { {"frameSmoothing", Adagio::Protocol::FrameSmoothingMax + 1} }, error));
	CHECK(error == expected);
	CHECK_FALSE(Adagio::UpdateEngineParams(params, { {"frameSmoothing", 4.5} }, error));
	CHECK_FALSE(Adagio::UpdateEngineParams(params, { {"frameSmoothing", "4"} }, error));
	CHECK_FALSE(Adagio::UpdateEngineParams(params, { {"frameSmoothing", 1LL << 40} }, error));
	CHECK(params.Analysis.FrameSmoothing == Adagio::Protocol::FrameSmoothingDefault);
}

TEST_CASE("Nothing changes unless every value given is valid")
{
	EngineParams params;
	std::string error;
	CHECK_FALSE(Adagio::UpdateEngineParams(params, { {"sampleRate", 16000}, {"frameLength", 8192}, {"frameSmoothing", 7}, {"hopSize", 100} }, error));
	CHECK(params.Analysis.SampleRate == (float)Adagio::Protocol::SampleRateDefault);
	CHECK(params.Analysis.FrameLength == Adagio::Protocol::FrameLengthDefault);
	CHECK(params.Analysis.HopSize == Adagio::Protocol::HopSizeDefault);
	CHECK(params.Analysis.FrameSmoothing == Adagio::Protocol::FrameSmoothingDefault);
}

TEST_CASE("An empty request, or one that isn't an object, is refused")
{
	EngineParams params;
	std::string error;
	CHECK_FALSE(Adagio::UpdateEngineParams(params, nlohmann::json::object(), error));
	CHECK(error == "setEngineParams needs a sampleRate, a frameLength, a hopSize or a frameSmoothing.");
	CHECK_FALSE(Adagio::UpdateEngineParams(params, { {"window", "Hann"} }, error));
	CHECK_FALSE(Adagio::UpdateEngineParams(params, 64, error));
}

TEST_CASE("Preprocess mixes to mono and resamples to the analysis rate")
{
	SUBCASE("downsampled, through the anti-alias filter")
	{
		const Adagio::AudioData source = MakeStereoTone(440.0f, 44100.0f, 1.0f);
		const kfr::univector<float> out = Adagio::AnalysisService::Preprocess(source, 4000.0f);
		CHECK(std::abs((int)out.size() - 4000) <= 1);
		CHECK(AllFinite(out));
	}

	SUBCASE("upsampled, unfiltered")
	{
		const Adagio::AudioData source = MakeStereoTone(440.0f, 11025.0f, 1.0f);
		const kfr::univector<float> out = Adagio::AnalysisService::Preprocess(source, 16000.0f);
		CHECK(std::abs((int)out.size() - 16000) <= 1);
		CHECK(AllFinite(out));
	}

	SUBCASE("at the source's own rate, only mixed")
	{
		const Adagio::AudioData source = MakeStereoTone(440.0f, 8000.0f, 0.5f);
		const kfr::univector<float> out = Adagio::AnalysisService::Preprocess(source, 8000.0f);
		REQUIRE(out.size() == 4000);
		CHECK(out[100] == doctest::Approx(source.PCMData[0][100]));
	}
}
