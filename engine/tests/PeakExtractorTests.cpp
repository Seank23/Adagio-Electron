// PeakExtractor's gates and score, which are ratios to each peak's local floor on either axis, and what it does on
// CQTProcessor's log axis alone: interpolate a peak's pitch and height in dB, and measure that height against the peak's
// valleys. The tone cases run CQTProcessor at its defaults, 36 bins an octave from 55 Hz, so A3 (220 Hz) is bin 72 and bin
// 73 is 33 cents above it, and move one tone off the grid a step at a time.
#include "../src/Analysis/AnalysisPipeline.h"
#include "../src/Analysis/AnalysisUtils.h"
#include "../src/Analysis/AudioFrame.h"
#include "../src/Analysis/FrequencyAxis.h"
#include "../src/Analysis/Stages/CQTProcessor.h"
#include "../src/Analysis/Stages/PeakExtractor.h"

#include <doctest/doctest.h>

#include <cmath>
#include <functional>
#include <memory>
#include <numbers>
#include <optional>
#include <vector>

namespace
{
	constexpr uint32_t SAMPLE_RATE = 8000;
	constexpr uint32_t FRAME_LENGTH = 8192;
	constexpr double A3_HZ = 220.0;
	constexpr int A3_BIN = 72;
	// Clean enough that the noise barely moves a peak's fitted pitch or height.
	constexpr double CLEAN_SNR_DB = 20.0;
	// Noisy enough that, as in music, the noise and not the tone's sidelobes sets the local floor, which otherwise moves
	// by a dB or two as the sidelobes do with the tone.
	constexpr double NOISY_SNR_DB = 10.0;
	// From on bin 72 to halfway to bin 73.
	const std::vector<double> OFFSETS_CENTS = { 0.0, 4.0, 8.0, 12.0, 15.0, 50.0 / 3.0 };

	double Cents(double hz, double reference)
	{
		return 1200.0 * std::log2(hz / reference);
	}

	double Detuned(double cents)
	{
		return A3_HZ * std::pow(2.0, cents / 1200.0);
	}

	// A sine with uniform noise snrDb below it from a fixed seed, so the spectrum has a floor for the peak to stand on.
	Adagio::AudioFrame ToneFrame(double hz, double snrDb)
	{
		Adagio::AudioFrame frame;
		frame.SampleRate = SAMPLE_RATE;
		frame.FrameLength = FRAME_LENGTH;
		frame.Timestamp = 1.0;
		frame.DeltaTime = 256.0 / SAMPLE_RATE;
		frame.Samples.resize(FRAME_LENGTH);

		// A sine's RMS is 1/sqrt(2), and uniform noise on [-1, 1) has an RMS of 1/sqrt(3).
		const double noiseScale = std::sqrt(1.5) * std::pow(10.0, -snrDb / 20.0);
		uint32_t state = 0x9E3779B9u;
		for (uint32_t i = 0; i < FRAME_LENGTH; i++)
		{
			state ^= state << 13;
			state ^= state >> 17;
			state ^= state << 5;
			const double noise = state / 2147483648.0 - 1.0;
			frame.Samples[i] = (float)(std::sin(2.0 * std::numbers::pi * hz * i / SAMPLE_RATE) + noiseScale * noise);
		}
		return frame;
	}

	// The peak PeakExtractor reports nearest the tone, at the stage's own defaults.
	Adagio::Peak PeakNear(double hz, double snrDb = CLEAN_SNR_DB)
	{
		Adagio::AnalysisPipeline pipeline;
		pipeline.AddStage(std::make_unique<Adagio::CQTProcessor>());
		pipeline.AddStage(std::make_unique<Adagio::PeakExtractor>());
		const std::unique_ptr<Adagio::AnalysisResult> result = pipeline.ProcessFrame(ToneFrame(hz, snrDb));
		REQUIRE(result->Context->FreqAxis->IsLog());
		REQUIRE(result->Context->FreqAxis->HzToBin((float)A3_HZ) == A3_BIN);

		std::optional<Adagio::Peak> nearest;
		for (const Adagio::Peak& peak : result->Context->Peaks)
		{
			if (!nearest || std::abs(Cents(peak.Frequency, hz)) < std::abs(Cents(nearest->Frequency, hz)))
				nearest = peak;
		}
		REQUIRE(nearest.has_value());
		REQUIRE(std::abs(Cents(nearest->Frequency, hz)) < 50.0);
		return *nearest;
	}

	// PeakExtractor at its defaults on a spectrum built by hand.
	std::vector<Adagio::Peak> PeaksOf(std::unique_ptr<Adagio::FrequencyAxis> axis, const std::vector<float>& magnitudes)
	{
		Adagio::AudioFrame frame;
		frame.SampleRate = SAMPLE_RATE;
		frame.FrameLength = FRAME_LENGTH;
		Adagio::PersistentData persistent;
		Adagio::AnalysisContext context{ frame, &persistent };
		context.FreqAxis = std::move(axis);
		context.Magnitudes.assign(magnitudes.begin(), magnitudes.end());

		Adagio::PeakExtractor stage;
		stage.ApplySettings(nlohmann::json::object());
		stage.Execute(&context);
		return context.Peaks;
	}

	std::unique_ptr<Adagio::FrequencyAxis> LogAxis()
	{
		return std::make_unique<Adagio::LogFrequencyAxis>(55.0f, 36);
	}

	std::unique_ptr<Adagio::FrequencyAxis> LinearAxis()
	{
		return std::make_unique<Adagio::LinearFrequencyAxis>(1.0f, 0.0f);
	}

	// Lays the shape over the spectrum with its middle on the given bin.
	void AddHill(std::vector<float>& spectrum, size_t top, const std::vector<float>& shape)
	{
		for (size_t i = 0; i < shape.size(); i++)
			spectrum[top - shape.size() / 2 + i] = shape[i];
	}

	// A floor of 1 with a hill five bins wide whose top stands 1 over its higher neighbour and 99 over its valleys: too
	// little prominence for the stage's default MIN_PROMINENCE (1.5 times the local median) against the neighbours, plenty
	// against the valleys.
	std::vector<float> BroadHill(size_t binCount, size_t top)
	{
		std::vector<float> spectrum(binCount, 1.0f);
		AddHill(spectrum, top, { 50.0f, 95.0f, 100.0f, 99.0f, 60.0f });
		return spectrum;
	}

	// A floor of 1 with hills 40 and 26 dB over it, the whole scaled by gain.
	std::vector<float> TwoHills(size_t binCount, size_t first, size_t second, float gain)
	{
		std::vector<float> spectrum(binCount, 1.0f);
		AddHill(spectrum, first, { 50.0f, 100.0f, 50.0f });
		AddHill(spectrum, second, { 10.0f, 20.0f, 10.0f });
		for (float& value : spectrum)
			value *= gain;
		return spectrum;
	}
}

TEST_CASE("PeakExtractor: on a log axis, a tone scores the same wherever it falls between bins")
{
	// Against its immediate neighbours a peak halfway between bins has almost no prominence, so its score, which weighs it in
	// the key and chord histograms, used to fall with its distance from the grid, and halfway it could be dropped.
	const float onBin = PeakNear(A3_HZ, NOISY_SNR_DB).Score;
	for (double cents : OFFSETS_CENTS)
	{
		CAPTURE(cents);
		CHECK(PeakNear(Detuned(cents), NOISY_SNR_DB).Score == doctest::Approx(onBin).epsilon(0.01));
	}
}

TEST_CASE("PeakExtractor: on a log axis, a tone's pitch is read to within a cent")
{
	// A parabola through the magnitudes was up to 2 cents out at 36 bins an octave.
	for (double cents : OFFSETS_CENTS)
	{
		CAPTURE(cents);
		const double hz = Detuned(cents);
		CHECK(std::abs(Cents(PeakNear(hz).Frequency, hz)) < 1.0);
	}
}

TEST_CASE("PeakExtractor: on a log axis, a tone's magnitude doesn't depend on where it falls between bins")
{
	// The top bin alone reads 1.7 dB low halfway between bins. A Hamming peak isn't quite a parabola in dB, so the fit reads
	// up to 0.4 dB high.
	const float onBin = PeakNear(A3_HZ).Magnitude;
	for (double cents : OFFSETS_CENTS)
	{
		CAPTURE(cents);
		CHECK(std::abs(Adagio::AnalysisUtils::ToDb(PeakNear(Detuned(cents)).Magnitude / onBin)) < 0.5f);
	}
}

TEST_CASE("PeakExtractor: a broad hill is a peak on a log axis, and on a linear axis still is not")
{
	const std::vector<Adagio::Peak> log = PeaksOf(LogAxis(), BroadHill(150, A3_BIN));
	REQUIRE(log.size() == 1);
	CHECK(log[0].Frequency == doctest::Approx(A3_HZ).epsilon(0.01));

	// The linear path is hps's, calibrated against neighbour prominence, and stays as it was.
	CHECK(PeaksOf(LinearAxis(), BroadHill(1000, 440)).empty());
}

TEST_CASE("PeakExtractor: a spectrum 80 dB quieter gives the same peaks with the same scores")
{
	// MIN_SNR, once a magnitude, and a fixed 0.05 of prominence were in the transform's own units, which move with the
	// recording's level and the frame length: at this level they dropped both hills. Every gate is now a ratio to the local
	// floor, on either axis.
	struct Case
	{
		const char* Axis;
		std::function<std::unique_ptr<Adagio::FrequencyAxis>()> MakeAxis;
		size_t Bins;
		size_t First;
		size_t Second;
	};
	const std::vector<Case> cases = {
		{ "linear", LinearAxis, 1000, 200, 600 },
		{ "log", LogAxis, 150, 36, 108 },
	};

	for (const Case& c : cases)
	{
		CAPTURE(c.Axis);
		const std::vector<Adagio::Peak> loud = PeaksOf(c.MakeAxis(), TwoHills(c.Bins, c.First, c.Second, 1.0f));
		const std::vector<Adagio::Peak> quiet = PeaksOf(c.MakeAxis(), TwoHills(c.Bins, c.First, c.Second, 1e-4f));
		REQUIRE(loud.size() == 2);
		REQUIRE(quiet.size() == loud.size());
		for (size_t p = 0; p < loud.size(); p++)
		{
			CAPTURE(p);
			CHECK(quiet[p].Frequency == doctest::Approx(loud[p].Frequency).epsilon(1e-5));
			CHECK(quiet[p].Score == doctest::Approx(loud[p].Score).epsilon(1e-4));
		}
	}
}

TEST_CASE("PeakExtractor: a peak's score doesn't depend on the spectrum far from it")
{
	// Scored against the median of the whole frame, a peak lost score when content far from it raised that median: here
	// from 1 to 30, though nothing within an octave of the peak changed. Against its own local floor it scores the same.
	std::vector<float> alone(1000, 1.0f);
	AddHill(alone, 220, { 50.0f, 100.0f, 50.0f });
	std::vector<float> busy = alone;
	std::fill(busy.begin() + 400, busy.end(), 30.0f);

	const std::vector<Adagio::Peak> before = PeaksOf(LinearAxis(), alone);
	const std::vector<Adagio::Peak> after = PeaksOf(LinearAxis(), busy);
	REQUIRE(before.size() == 1);
	REQUIRE(after.size() == 1);
	CHECK(after[0].Score == doctest::Approx(before[0].Score).epsilon(1e-5));
}

TEST_CASE("AnalysisUtils::HigherValley finds the higher of a peak's two valleys")
{
	const std::vector<float> spectrum = { 0.0f, 1.0f, 5.0f, 9.0f, 5.0f, 2.0f, 3.0f, 0.0f };
	// Valleys of 0 at bin 0 and 2 at bin 5.
	CHECK(Adagio::AnalysisUtils::HigherValley(spectrum, 3, 8) == 2.0f);
	// A small peak on a larger one's skirt rises only out of the dip between them.
	CHECK(Adagio::AnalysisUtils::HigherValley(spectrum, 6, 8) == 2.0f);
}

TEST_CASE("AnalysisUtils::HigherValley walks past a flat top and no further than maxSpan")
{
	const std::vector<float> flatTop = { 0.0f, 9.0f, 9.0f, 1.0f, 0.0f };
	CHECK(Adagio::AnalysisUtils::HigherValley(flatTop, 1, 8) == 0.0f);

	const std::vector<float> slopes = { 0.0f, 1.0f, 2.0f, 3.0f, 9.0f, 3.0f, 2.0f, 1.0f, 0.0f };
	CHECK(Adagio::AnalysisUtils::HigherValley(slopes, 4, 2) == 2.0f);
}

TEST_CASE("AnalysisUtils::LogParabolicPeak finds a Gaussian's top exactly")
{
	// A Gaussian is a parabola in dB.
	constexpr float top = 10.3f;
	constexpr float height = 2.0f;
	constexpr float width = 1.5f;
	std::vector<float> gaussian(21);
	for (size_t k = 0; k < gaussian.size(); k++)
		gaussian[k] = height * std::exp(-((float)k - top) * ((float)k - top) / (2.0f * width * width));

	const Adagio::InterpolatedPeak peak = Adagio::AnalysisUtils::LogParabolicPeak(gaussian, 10);
	CHECK(peak.Bin == doctest::Approx(top).epsilon(1e-4));
	CHECK(peak.Magnitude == doctest::Approx(height).epsilon(1e-4));
}
