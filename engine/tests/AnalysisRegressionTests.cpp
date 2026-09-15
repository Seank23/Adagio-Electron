// Regression tests for the analysis findings in the architecture review.
//
// The cases marked doctest::should_fail reproduce bugs that Phase 2 fixes. They
// are decorated rather than left plainly failing so `ctest` can gate merges from
// Phase 0 onward. When Phase 2 lands the fix, the decoration itself starts
// failing, which is the signal to delete it and keep the assertion.
#include "../src/Analysis/AnalysisPipeline.h"
#include "../src/Analysis/AnalysisUtils.h"
#include "../src/Analysis/AudioFrame.h"
#include "../src/Analysis/ChordPredictor.h"
#include "../src/Analysis/FFTProcessor.h"
#include "../src/Analysis/KeyDetector.h"
#include "../src/Analysis/NoteDetector.h"
#include "../src/Analysis/PeakExtractor.h"

#include <doctest/doctest.h>

#include <cmath>
#include <memory>
#include <string>
#include <vector>

namespace
{
	constexpr float Pi = 3.14159265358979323846f;

	// A fundamental plus three harmonics at 1/k amplitude, i.e. a plucked-string
	// shape rather than a bare sine. PeakExtractor normalises its scores across
	// the candidates in a frame, so a frame holding exactly one candidate scores
	// it 0 and drops it - a single pure sine yields no peaks at all.
	Adagio::AudioFrame MakeToneFrame(float fundamentalHz, uint32_t sampleRate, uint32_t frameLength)
	{
		Adagio::AudioFrame frame;
		frame.SampleRate = sampleRate;
		frame.FrameLength = frameLength;
		frame.Timestamp = 1.0;
		frame.Samples.resize(frameLength);
		for (uint32_t i = 0; i < frameLength; ++i)
		{
			float sample = 0.0f;
			for (int harmonic = 1; harmonic <= 4; ++harmonic)
			{
				const float hz = fundamentalHz * static_cast<float>(harmonic);
				sample += std::sin(2.0f * Pi * hz * static_cast<float>(i) / static_cast<float>(sampleRate)) / static_cast<float>(harmonic);
			}
			frame.Samples[i] = sample;
		}
		return frame;
	}

	// Equal temperament relative to A440, so the test states the pitch it means
	// rather than a magic number.
	float PitchHz(int midi, float detuneCents = 0.0f)
	{
		return 440.0f * std::pow(2.0f, (static_cast<float>(midi) - 69.0f) / 12.0f + detuneCents / 1200.0f);
	}

	// KeyDetector reads an uninitialised std::array (A3). Leaving known garbage on
	// the stack first makes that read observable instead of accidentally zero.
	void DirtyStack()
	{
		volatile float scratch[4096];
		for (int i = 0; i < 4096; ++i)
			scratch[i] = static_cast<float>(i) * 13.7f + 1.0f;
	}

	Adagio::Note MakeNote(int midi, float frequency, float score, double timestamp)
	{
		Adagio::Note note;
		note.Name = Adagio::NoteNames.at(midi % 12);
		note.Midi = midi;
		note.PeakInfo = { frequency, score, score };
		note.ErrorCents = 0.0f;
		note.Timestamp = timestamp;
		return note;
	}
}

TEST_CASE("A1: a 440 Hz tone is reported as A4" * doctest::should_fail())
{
	// An N-sample FFT has bins SR/N apart, but the peak stage divides by the
	// truncated magnitude count (N/2), so every frequency comes out doubled.
	const Adagio::AudioFrame frame = MakeToneFrame(440.0f, 8000, 4096);

	Adagio::AnalysisPipeline pipeline;
	pipeline.AddStage(std::make_unique<Adagio::FFTProcessor>());
	pipeline.AddStage(std::make_unique<Adagio::PeakExtractor>());
	pipeline.AddStage(std::make_unique<Adagio::NoteDetector>());

	std::unique_ptr<Adagio::AnalysisResult> result = pipeline.ProcessFrame(frame);
	const std::vector<Adagio::Note>& notes = result->Context->Notes;
	REQUIRE_FALSE(notes.empty());

	const Adagio::Note* loudest = &notes[0];
	for (const Adagio::Note& note : notes)
	{
		if (note.PeakInfo.Magnitude > loudest->PeakInfo.Magnitude)
			loudest = &note;
	}

	CHECK(loudest->PeakInfo.Frequency == doctest::Approx(440.0f).epsilon(0.01));
	CHECK(loudest->Name == "A4");
}

TEST_CASE("PeakExtractor: an isolated peak is not scored away" * doctest::should_fail())
{
	// Peak scores are min-max normalised across the candidates in the frame, so a
	// lone candidate normalises to 0 on all three components and is then dropped
	// by SCORE_THRESHOLD. A pure sine therefore yields no peaks at all, and in any
	// frame the weakest candidate is scored 0 on whichever components it is last
	// on. Not listed in the review; found while building the A1 fixture.
	const Adagio::AudioFrame frame = MakeToneFrame(440.0f, 8000, 4096);
	Adagio::AudioFrame sine = frame;
	for (uint32_t i = 0; i < sine.FrameLength; ++i)
		sine.Samples[i] = std::sin(2.0f * Pi * 440.0f * static_cast<float>(i) / 8000.0f);

	Adagio::AnalysisPipeline pipeline;
	pipeline.AddStage(std::make_unique<Adagio::FFTProcessor>());
	pipeline.AddStage(std::make_unique<Adagio::PeakExtractor>());

	std::unique_ptr<Adagio::AnalysisResult> result = pipeline.ProcessFrame(sine);
	CHECK(result->MaxMagnitude > 100.0f);
	CHECK(result->Context->Peaks.size() == 1);
}

TEST_CASE("A2: a C4 that is 20 cents flat is still a C4" * doctest::should_fail())
{
	// The pitch class comes from a rounded semitone but the octave comes from a
	// floor, so a flat C rounds up an octave and reports ~1195 cents of error.
	Adagio::AudioFrame frame;
	frame.SampleRate = 8000;
	frame.FrameLength = 4096;
	frame.Timestamp = 1.0;

	Adagio::PersistentData persistent;
	Adagio::AnalysisContext context{ frame, &persistent };
	context.Settings = nlohmann::json::object();
	context.Peaks.push_back({ PitchHz(60, -20.0f), 1.0f, 1.0f });

	Adagio::NoteDetector detector;
	detector.Execute(&context);

	REQUIRE(context.Notes.size() == 1);
	CHECK(context.Notes[0].Name == "C4");
	CHECK(context.Notes[0].ErrorCents == doctest::Approx(-20.0f).epsilon(0.05));
}

TEST_CASE("A2: a G4 that is 20 cents flat is a G4")
{
	// The control for the case above: every pitch class except C survives,
	// which is what biases key and chord detection away from C.
	Adagio::AudioFrame frame;
	frame.SampleRate = 8000;
	frame.FrameLength = 4096;
	frame.Timestamp = 1.0;

	Adagio::PersistentData persistent;
	Adagio::AnalysisContext context{ frame, &persistent };
	context.Settings = nlohmann::json::object();
	context.Peaks.push_back({ PitchHz(67, -20.0f), 1.0f, 1.0f });

	Adagio::NoteDetector detector;
	detector.Execute(&context);

	REQUIRE(context.Notes.size() == 1);
	CHECK(context.Notes[0].Name == "G4");
}

TEST_CASE("A3: a fixed C major histogram detects C Major" * doctest::may_fail())
{
	// KeyDetector accumulates into an uninitialised std::array<double,12>, so the
	// answer depends on whatever the stack held. With the scratch values below it
	// currently reports B Major. may_fail rather than should_fail because the
	// result is undefined behaviour: another toolchain may leave zeroes there and
	// get the right answer by luck. Phase 2 (`similarity{}`) makes it deterministic,
	// and this decoration comes off then.
	Adagio::AudioFrame frame;
	frame.SampleRate = 8000;
	frame.FrameLength = 4096;
	frame.Timestamp = 1.0;

	Adagio::PersistentData persistent;
	const int scale[] = { 0, 2, 4, 5, 7, 9, 11 };
	const float weights[] = { 10.0f, 2.0f, 6.0f, 3.0f, 8.0f, 4.0f, 2.0f };
	for (int i = 0; i < 7; ++i)
		persistent.RollingNotes.push_back(MakeNote(48 + scale[i], PitchHz(60 + scale[i]), weights[i], 0.5));

	Adagio::AnalysisContext context{ frame, &persistent };
	context.Settings = nlohmann::json::object();

	Adagio::KeyDetector detector;
	DirtyStack();
	detector.Execute(&context);

	CHECK(context.DetectedKey == "C Major");
}

TEST_CASE("A3: key detection is deterministic for identical input")
{
	Adagio::AudioFrame frame;
	frame.SampleRate = 8000;
	frame.FrameLength = 4096;
	frame.Timestamp = 1.0;

	Adagio::PersistentData persistent;
	const int scale[] = { 0, 2, 4, 5, 7, 9, 11 };
	const float weights[] = { 10.0f, 2.0f, 6.0f, 3.0f, 8.0f, 4.0f, 2.0f };
	for (int i = 0; i < 7; ++i)
		persistent.RollingNotes.push_back(MakeNote(48 + scale[i], PitchHz(60 + scale[i]), weights[i], 0.5));

	std::string first;
	for (int run = 0; run < 8; ++run)
	{
		Adagio::AnalysisContext context{ frame, &persistent };
		context.Settings = nlohmann::json::object();

		Adagio::KeyDetector detector;
		DirtyStack();
		detector.Execute(&context);

		if (run == 0)
			first = context.DetectedKey;
		else
			CHECK(context.DetectedKey == first);
	}
}

TEST_CASE("A5: a dominant 7th suspended chord keeps its 7th" * doctest::should_fail())
{
	// The sus branches remove the suspended tone and then test for it, so the
	// 7th/9th qualities below them are unreachable.
	Adagio::ChordPredictor predictor;
	int fifthOmitted = 0;

	CHECK(predictor.GetChordQuality({ 5, 7, 10 }, fifthOmitted) == "7sus4");
	CHECK(predictor.GetChordQuality({ 2, 7, 10 }, fifthOmitted) == "7sus2");
}

TEST_CASE("A5: chord qualities that the interval table gets right today")
{
	// Anchors, so the Phase 2 rewrite of the sus branches has to keep these.
	Adagio::ChordPredictor predictor;
	int fifthOmitted = 0;

	CHECK(predictor.GetChordQuality({ 4, 7 }, fifthOmitted) == "");
	CHECK(predictor.GetChordQuality({ 4, 7, 10 }, fifthOmitted) == "7");
	CHECK(predictor.GetChordQuality({ 5, 7 }, fifthOmitted) == "sus4");
	CHECK(predictor.GetChordQuality({ 2, 7 }, fifthOmitted) == "sus2");
}
