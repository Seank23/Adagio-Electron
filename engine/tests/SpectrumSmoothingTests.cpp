#include "../src/Analysis/AnalysisPipeline.h"
#include "../src/Analysis/AudioFrame.h"
#include "../src/Analysis/SpectrumSmoother.h"
#include "../src/Analysis/Stages/CQTProcessor.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace
{
	constexpr double FRAME_SECONDS = 128.0 / 8000.0;

	kfr::univector<float> Filled(size_t size, float value)
	{
		kfr::univector<float> values(size);
		std::fill(values.begin(), values.end(), value);
		return values;
	}

	Adagio::AnalysisContext RunCQT(Adagio::CQTProcessor& stage, Adagio::AudioFrame& frame, Adagio::PersistentData& persistent, const nlohmann::json& settings)
	{
		frame.SampleRate = 8000;
		frame.FrameLength = 4096;
		frame.Samples.resize(4096);
		for (size_t i = 0; i < frame.Samples.size(); i++)
			frame.Samples[i] = std::sin(2.0f * 3.14159265f * 440.0f * (float)i / 8000.0f);

		stage.ApplySettings(settings);
		Adagio::AnalysisContext context{ frame, &persistent };
		context.Samples = frame.Samples;
		stage.Execute(&context);
		return context;
	}
}

TEST_CASE("Spectrum smoothing: N frames moves a step as far as an N-frame average's mean lag")
{
	// A one-pole filter with alpha 2 / (N + 1) lags (N - 1) / 2 frames and leaves 1/N of the noise, like an N-frame average.
	Adagio::SpectrumSmoother smoother;
	kfr::univector<float> frame = Filled(3, 0.0f);
	smoother.Apply(frame, {}, 4, FRAME_SECONDS, FRAME_SECONDS);

	frame = Filled(3, 1.0f);
	smoother.Apply(frame, {}, 4, FRAME_SECONDS, FRAME_SECONDS);
	for (float value : frame)
		CHECK(value == doctest::Approx(2.0f / 5.0f));
}

TEST_CASE("Spectrum smoothing: one frame and no per-bin time passes the frame through")
{
	Adagio::SpectrumSmoother smoother;
	kfr::univector<float> frame = Filled(2, 5.0f);
	smoother.Apply(frame, {}, 1, FRAME_SECONDS, FRAME_SECONDS);

	frame = Filled(2, 1.0f);
	smoother.Apply(frame, {}, 1, FRAME_SECONDS, FRAME_SECONDS);
	CHECK(frame[0] == 1.0f);
	CHECK(frame[1] == 1.0f);
}

TEST_CASE("Spectrum smoothing: a bin with its own time lags further than one without")
{
	Adagio::SpectrumSmoother smoother;
	const std::vector<float> binSeconds = { 0.0f, 0.2f };
	kfr::univector<float> frame = Filled(2, 0.0f);
	smoother.Apply(frame, binSeconds, 1, FRAME_SECONDS, FRAME_SECONDS);

	frame = Filled(2, 1.0f);
	smoother.Apply(frame, binSeconds, 1, FRAME_SECONDS, FRAME_SECONDS);
	CHECK(frame[0] == 1.0f);
	// A lag of half the bin's time: 0.1 s, against a 16 ms frame.
	CHECK(frame[1] == doctest::Approx(FRAME_SECONDS / (FRAME_SECONDS + 0.1)));
}

TEST_CASE("Spectrum smoothing: per-bin times of the wrong size are ignored, and a new bin count starts afresh")
{
	Adagio::SpectrumSmoother smoother;
	const std::vector<float> binSeconds = { 1.0f };
	kfr::univector<float> frame = Filled(2, 0.0f);
	smoother.Apply(frame, binSeconds, 1, FRAME_SECONDS, FRAME_SECONDS);

	frame = Filled(2, 1.0f);
	smoother.Apply(frame, binSeconds, 1, FRAME_SECONDS, FRAME_SECONDS);
	CHECK(frame[0] == 1.0f);

	frame = Filled(3, 7.0f);
	smoother.Apply(frame, {}, 4, FRAME_SECONDS, FRAME_SECONDS);
	CHECK(frame[0] == 7.0f);
}

TEST_CASE("CQTProcessor: the higher the bin, the more smoothing it asks for")
{
	Adagio::CQTProcessor stage;
	stage.Initialise();
	Adagio::AudioFrame frame;
	Adagio::PersistentData persistent;
	const Adagio::AnalysisContext context = RunCQT(stage, frame, persistent, nlohmann::json{ { "HIGH_FREQ_SMOOTHING", 1.0f } });

	const std::vector<float>& seconds = context.BinSmoothingSeconds;
	REQUIRE(seconds.size() == context.Magnitudes.size());
	REQUIRE(seconds.size() > 2);
	CHECK(seconds.front() == 0.0f);
	for (size_t i = 1; i < seconds.size(); i++)
		CHECK(seconds[i] >= seconds[i - 1]);
	// At full strength the top bin is smoothed by nearly the lowest bin's window, which is at most the 512 ms frame.
	CHECK(seconds.back() > 0.4f);
	CHECK(seconds.back() < 4096.0f / 8000.0f);
}

TEST_CASE("CQTProcessor: no high-frequency smoothing asks for none")
{
	Adagio::CQTProcessor stage;
	stage.Initialise();
	Adagio::AudioFrame frame;
	Adagio::PersistentData persistent;
	const Adagio::AnalysisContext context = RunCQT(stage, frame, persistent, nlohmann::json{ { "HIGH_FREQ_SMOOTHING", 0.0f } });
	CHECK(context.BinSmoothingSeconds.empty());
}
