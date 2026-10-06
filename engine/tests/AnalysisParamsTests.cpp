// The analysis parameters a client or the command line may set, and the preprocessing a
// new sample rate runs the track through again.
#include "../src/Analysis/AnalysisParams.h"
#include "../src/Analysis/AnalysisService.h"
#include "../src/IO/AudioData.h"

#include <doctest/doctest.h>

#include <cmath>
#include <string>

namespace
{
	using Adagio::AnalysisParams;

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
	const AnalysisParams params;
	CHECK(params.SampleRate == (float)Adagio::Protocol::SampleRateDefault);
	CHECK(params.FrameLength == Adagio::Protocol::FrameLengthDefault);
	CHECK(params.HopSize == Adagio::Protocol::HopSizeDefault);

	const nlohmann::json json = Adagio::AnalysisParamsJson(params);
	CHECK(json == nlohmann::json{ {"sampleRate", 8000}, {"frameLength", 4096}, {"hopSize", 64} });
}

TEST_CASE("Any one value, or several, may be set")
{
	AnalysisParams params;
	std::string error;

	REQUIRE(Adagio::UpdateAnalysisParams(params, { {"hopSize", 128} }, error));
	CHECK(params.HopSize == 128);
	CHECK(params.FrameLength == 4096);

	REQUIRE(Adagio::UpdateAnalysisParams(params, { {"sampleRate", 16000}, {"frameLength", 8192} }, error));
	CHECK(params.SampleRate == 16000.0f);
	CHECK(params.FrameLength == 8192);
	CHECK(params.HopSize == 128);

	for (int rate : Adagio::Protocol::SampleRates)
		CHECK(Adagio::UpdateAnalysisParams(params, { {"sampleRate", rate} }, error));
	for (int length : Adagio::Protocol::FrameLengths)
		CHECK(Adagio::UpdateAnalysisParams(params, { {"frameLength", length} }, error));
	for (int hop : Adagio::Protocol::HopSizes)
		CHECK(Adagio::UpdateAnalysisParams(params, { {"hopSize", hop} }, error));
}

TEST_CASE("A value off its list is refused with the list")
{
	AnalysisParams params;
	std::string error;

	CHECK_FALSE(Adagio::UpdateAnalysisParams(params, { {"hopSize", 100} }, error));
	CHECK(error == "hopSize is one of 32, 64, 128 or 256.");

	CHECK_FALSE(Adagio::UpdateAnalysisParams(params, { {"sampleRate", 44100} }, error));
	CHECK(error == "sampleRate is one of 4000, 8000 or 16000.");

	CHECK_FALSE(Adagio::UpdateAnalysisParams(params, { {"frameLength", "4096"} }, error));
	CHECK(error == "frameLength is one of 2048, 4096 or 8192.");

	// 8000.5 is not an integer, and nor, on the wire, is 8000.0.
	CHECK_FALSE(Adagio::UpdateAnalysisParams(params, { {"sampleRate", 8000.5} }, error));
	CHECK_FALSE(Adagio::UpdateAnalysisParams(params, { {"sampleRate", 8000.0} }, error));
	CHECK_FALSE(Adagio::UpdateAnalysisParams(params, { {"hopSize", -64} }, error));
}

TEST_CASE("Nothing changes unless every value given is valid")
{
	AnalysisParams params;
	std::string error;
	CHECK_FALSE(Adagio::UpdateAnalysisParams(params, { {"sampleRate", 16000}, {"frameLength", 8192}, {"hopSize", 100} }, error));
	CHECK(params.SampleRate == 8000.0f);
	CHECK(params.FrameLength == 4096);
	CHECK(params.HopSize == 64);
}

TEST_CASE("An empty request, or one that isn't an object, is refused")
{
	AnalysisParams params;
	std::string error;
	CHECK_FALSE(Adagio::UpdateAnalysisParams(params, nlohmann::json::object(), error));
	CHECK(error == "setEngineParams needs a sampleRate, a frameLength or a hopSize.");
	CHECK_FALSE(Adagio::UpdateAnalysisParams(params, { {"window", "Hann"} }, error));
	CHECK_FALSE(Adagio::UpdateAnalysisParams(params, 64, error));
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
