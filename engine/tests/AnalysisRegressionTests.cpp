// Regression tests for the analysis findings in the architecture review.
//
// The cases marked doctest::should_fail reproduce bugs that a later change fixes:
// Phase 2 for most, the Note Selection plan for A10. They are decorated rather
// than left plainly failing so `ctest` can gate merges in the meantime. When the
// fix lands, the decoration itself starts failing, which is the signal to delete
// it and keep the assertion.
#include "../src/Analysis/AnalysisPipeline.h"
#include "../src/Analysis/AnalysisService.h"
#include "../src/Analysis/AnalysisUtils.h"
#include "../src/Analysis/AudioFrame.h"
#include "../src/Analysis/Stages/ChordPredictor.h"
#include "../src/Analysis/Stages/FFTProcessor.h"
#include "../src/Analysis/Stages/KeyDetector.h"
#include "../src/Analysis/Stages/NoteDetector.h"
#include "../src/Analysis/Stages/PeakExtractor.h"
#include "../src/Analysis/Stages/SpectrumFilterProcessor.h"

// The Note Selection plan's three stages, which its third commit adds. The cases that
// read them directly compile in once they exist; take the guard off then.
#if __has_include("../src/Analysis/NoteSelector.h")
#define ADAGIO_NOTE_SELECTION 1
#include "../src/Analysis/Stages/HarmonicGrouper.h"
#include "../src/Analysis/Stages/NoteSelector.h"
#include "../src/Analysis/Stages/PartialExtractor.h"
#endif

#include <doctest/doctest.h>

#include <array>
#include <cmath>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace
{
	constexpr float Pi = 3.14159265358979323846f;
	// One production hop at 1x: 256 samples of the 8 kHz analysis stream.
	constexpr double HopSeconds = 256.0 / 8000.0;

	// A fundamental plus three harmonics at 1/k amplitude, i.e. a plucked-string
	// shape rather than a bare sine.
	Adagio::AudioFrame MakeToneFrame(float fundamentalHz, uint32_t sampleRate, uint32_t frameLength)
	{
		Adagio::AudioFrame frame;
		frame.SampleRate = sampleRate;
		frame.FrameLength = frameLength;
		frame.Timestamp = 1.0;
		frame.DeltaTime = HopSeconds;
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

	// Pure sines at the given pitches, with no harmonics to be mistaken for chord
	// tones of their own.
	Adagio::AudioFrame MakeChordFrame(const std::vector<int>& midiNotes, uint32_t sampleRate, uint32_t frameLength)
	{
		Adagio::AudioFrame frame;
		frame.SampleRate = sampleRate;
		frame.FrameLength = frameLength;
		frame.Timestamp = 1.0;
		frame.DeltaTime = HopSeconds;
		frame.Samples.resize(frameLength);
		for (uint32_t i = 0; i < frameLength; ++i)
		{
			float sample = 0.0f;
			for (int midi : midiNotes)
				sample += std::sin(2.0f * Pi * PitchHz(midi) * static_cast<float>(i) / static_cast<float>(sampleRate));
			frame.Samples[i] = sample;
		}
		return frame;
	}

	std::unique_ptr<Adagio::AnalysisResult> RunNotePipeline(const Adagio::AudioFrame& frame)
	{
		Adagio::AnalysisPipeline pipeline;
		pipeline.AddStage(std::make_unique<Adagio::FFTProcessor>());
		pipeline.AddStage(std::make_unique<Adagio::PeakExtractor>());
		pipeline.AddStage(std::make_unique<Adagio::NoteDetector>());
		return pipeline.ProcessFrame(frame);
	}

	std::unique_ptr<Adagio::AnalysisResult> RunChordPipeline(const Adagio::AudioFrame& frame)
	{
		Adagio::AnalysisPipeline pipeline;
		pipeline.AddStage(std::make_unique<Adagio::FFTProcessor>());
		pipeline.AddStage(std::make_unique<Adagio::PeakExtractor>());
		pipeline.AddStage(std::make_unique<Adagio::NoteDetector>());
		pipeline.AddStage(std::make_unique<Adagio::ChordPredictor>());
		return pipeline.ProcessFrame(frame);
	}

	const Adagio::Note& LoudestNote(const std::vector<Adagio::Note>& notes)
	{
		const Adagio::Note* loudest = &notes[0];
		for (const Adagio::Note& note : notes)
		{
			if (note.PeakInfo.Magnitude > loudest->PeakInfo.Magnitude)
				loudest = &note;
		}
		return *loudest;
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

TEST_CASE("A1: a 440 Hz tone is reported as A4")
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

TEST_CASE("A9: an isolated peak is not scored away")
{
	// Peak scores used to be min-max normalised across the candidates in the frame,
	// so a lone candidate normalised to 0 on all three components and SCORE_THRESHOLD
	// dropped it: a pure sine yielded no peaks at all. Scores are now taken against
	// the frame's own noise floor and spectrum maximum.
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

TEST_CASE("A2: a C4 that is 20 cents flat is still a C4")
{
	// The pitch class comes from a rounded semitone but the octave comes from a
	// floor, so a flat C rounds up an octave and reports ~1195 cents of error.
	Adagio::AudioFrame frame;
	frame.SampleRate = 8000;
	frame.FrameLength = 4096;
	frame.Timestamp = 1.0;

	Adagio::PersistentData persistent;
	Adagio::AnalysisContext context{ frame, &persistent };
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
	context.Peaks.push_back({ PitchHz(67, -20.0f), 1.0f, 1.0f });

	Adagio::NoteDetector detector;
	detector.Execute(&context);

	REQUIRE(context.Notes.size() == 1);
	CHECK(context.Notes[0].Name == "G4");
}

TEST_CASE("A3: a fixed C major histogram detects C Major")
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
	frame.DeltaTime = HopSeconds;

	Adagio::PersistentData persistent;
	Adagio::AnalysisContext context{ frame, &persistent };
	const int scale[] = { 0, 2, 4, 5, 7, 9, 11 };
	const float weights[] = { 10.0f, 2.0f, 6.0f, 3.0f, 8.0f, 4.0f, 2.0f };
	for (int i = 0; i < 7; ++i)
		context.Notes.push_back(MakeNote(48 + scale[i], PitchHz(60 + scale[i]), weights[i], 1.0));

	Adagio::KeyDetector detector;
	DirtyStack();
	detector.Execute(&context);

	CHECK(context.PredictedKey.Name == "C Major");
}

TEST_CASE("A3: no notes, no key")
{
	// An all-zero histogram used to name key 0, so silence or a detuned track read as C Major.
	Adagio::AudioFrame frame;
	frame.SampleRate = 8000;
	frame.FrameLength = 4096;
	frame.Timestamp = 1.0;
	frame.DeltaTime = HopSeconds;

	Adagio::PersistentData persistent;
	Adagio::AnalysisContext context{ frame, &persistent };
	Adagio::KeyDetector detector;
	detector.Execute(&context);

	CHECK(context.PredictedKey.Name.empty());
	CHECK(context.PredictedKey.TonicClass == -1);
}

TEST_CASE("A3: key detection is deterministic for identical input")
{
	Adagio::AudioFrame frame;
	frame.SampleRate = 8000;
	frame.FrameLength = 4096;
	frame.Timestamp = 1.0;
	frame.DeltaTime = HopSeconds;

	const int scale[] = { 0, 2, 4, 5, 7, 9, 11 };
	const float weights[] = { 10.0f, 2.0f, 6.0f, 3.0f, 8.0f, 4.0f, 2.0f };

	std::string first;
	for (int run = 0; run < 8; ++run)
	{
		Adagio::PersistentData persistent;
		Adagio::AnalysisContext context{ frame, &persistent };
		for (int i = 0; i < 7; ++i)
			context.Notes.push_back(MakeNote(48 + scale[i], PitchHz(60 + scale[i]), weights[i], 1.0));

		Adagio::KeyDetector detector;
		DirtyStack();
		detector.Execute(&context);

		if (run == 0)
			first = context.PredictedKey.Name;
		else
			CHECK(context.PredictedKey.Name == first);
	}
}

TEST_CASE("A5: a dominant 7th suspended chord keeps its 7th")
{
	// The sus branches used to remove the suspended tone and then test for it, so
	// the 7th qualities below them were unreachable and the 7 was eaten.
	Adagio::ChordPredictor predictor;
	int fifthOmitted = 0;

	CHECK(predictor.GetChordQuality({ 5, 7, 10 }, fifthOmitted) == "7sus4");
	CHECK(predictor.GetChordQuality({ 2, 7, 10 }, fifthOmitted) == "7sus2");
}

TEST_CASE("A5: chord qualities by interval set")
{
	// Intervals are semitones above the root and never include the root itself,
	// which is how ChordPredictor::Execute builds them. The last case has no fifth
	// at all, so the quality is only reached on the retry that assumes one.
	struct QualityCase
	{
		std::vector<int> Intervals;
		std::string Quality;
		int FifthOmitted;
	};

	const std::vector<QualityCase> cases = {
		{ { 7 }, "5", 0 },
		{ { 4, 7 }, "", 0 },
		{ { 3, 7 }, "m", 0 },
		{ { 3, 6 }, "dim", 0 },
		{ { 4, 8 }, "aug", 0 },
		{ { 4, 7, 10 }, "7", 0 },
		{ { 4, 7, 11 }, "maj7", 0 },
		{ { 3, 7, 10 }, "m7", 0 },
		{ { 3, 6, 10 }, "m7b5", 0 },
		{ { 2, 7 }, "sus2", 0 },
		{ { 5, 7 }, "sus4", 0 },
		{ { 2, 7, 10 }, "7sus2", 0 },
		{ { 5, 7, 10 }, "7sus4", 0 },
		{ { 2, 5, 7, 10 }, "9sus4", 0 },
		{ { 4, 10 }, "7", 1 },
	};

	Adagio::ChordPredictor predictor;
	for (const QualityCase& testCase : cases)
	{
		CAPTURE(testCase.Quality);
		int fifthOmitted = -1;
		CHECK(predictor.GetChordQuality(testCase.Intervals, fifthOmitted) == testCase.Quality);
		CHECK(fifthOmitted == testCase.FifthOmitted);
	}
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

TEST_CASE("A6: chord frames expire on absolute distance from the frame")
{
	// After a backwards seek the stored frames sit in the future, where a signed
	// compare (timestamp - frame.Timestamp > window) is negative, so they would
	// never expire and would keep steering the chord until playback caught up.
	auto expire = [](Adagio::PersistentData& persistent, double frameTimestamp)
	{
		Adagio::AudioFrame frame;
		frame.SampleRate = 8000;
		frame.FrameLength = 4096;
		frame.Timestamp = frameTimestamp;
		frame.DeltaTime = HopSeconds;

		Adagio::AnalysisContext context{ frame, &persistent };

		Adagio::ChordPredictor predictor;
		predictor.Execute(&context);
	};

	// ChordPredictor's ROLLING_WINDOW defaults to 0.5 s. The frame being analysed is
	// always kept, so a window emptied of everything else still holds one.
	Adagio::PersistentData afterSeek;
	afterSeek.ChordFrames.push_back({ 40.0, HopSeconds, { MakeNote(60, PitchHz(60), 1.0f, 40.0) } });
	afterSeek.ChordFrames.push_back({ 41.0, HopSeconds, { MakeNote(64, PitchHz(64), 1.0f, 41.0) } });
	expire(afterSeek, 1.0);
	CHECK(afterSeek.ChordFrames.size() == 1);

	Adagio::PersistentData withinWindow;
	withinWindow.ChordFrames.push_back({ 0.6, HopSeconds, { MakeNote(60, PitchHz(60), 1.0f, 0.6) } });
	withinWindow.ChordFrames.push_back({ 0.9, HopSeconds, { MakeNote(64, PitchHz(64), 1.0f, 0.9) } });
	expire(withinWindow, 1.0);
	CHECK(withinWindow.ChordFrames.size() == 3);
}

TEST_CASE("A6: the chord window reaches the oldest note in range")
{
	// The reverse walk compared an unsigned index with >= 0, which never ends: once
	// every note was inside the window it ran off the front of the deque.
	Adagio::AudioFrame frame;
	frame.SampleRate = 8000;
	frame.FrameLength = 4096;
	frame.Timestamp = 1.0;
	frame.DeltaTime = HopSeconds;

	// ChordPredictor's ROLLING_WINDOW defaults to 0.5 s, so all three are in range.
	Adagio::PersistentData persistent;
	persistent.ChordFrames.push_back({ 0.9, HopSeconds, { MakeNote(60, PitchHz(60), 3.0f, 0.9) } });
	persistent.ChordFrames.push_back({ 0.95, HopSeconds, { MakeNote(64, PitchHz(64), 2.0f, 0.95) } });

	Adagio::AnalysisContext context{ frame, &persistent };
	context.Notes.push_back(MakeNote(67, PitchHz(67), 1.0f, 1.0));

	Adagio::ChordPredictor predictor;
	predictor.Execute(&context);

	CHECK(context.ChordFrequencyHistogram.size() == 3);

	bool predictedCMajor = false;
	for (const Adagio::Chord& chord : context.PredictedChords)
	{
		if (chord.Name == "C")
			predictedCMajor = true;
	}
	CHECK(predictedCMajor);
}

TEST_CASE("A8: the playhead estimate follows speed and stands still while paused")
{
	const double lastFrame = 1000.0;
	const double playbackTime = 12.0;

	// Paused: the delta is the length of the pause, so nothing is carried forward.
	CHECK(Adagio::AnalysisService::ExtrapolatePlayhead(playbackTime, lastFrame, lastFrame + 30.0, 1.0, false) == doctest::Approx(playbackTime));
	// Before the first audio callback the stored stamp is still 0, and the delta
	// would be the time since the clock's epoch.
	CHECK(Adagio::AnalysisService::ExtrapolatePlayhead(playbackTime, 0.0, 1.7e9, 1.0, true) == doctest::Approx(playbackTime));
	// Playing: source seconds advance at the stretch speed, not at wall speed.
	CHECK(Adagio::AnalysisService::ExtrapolatePlayhead(playbackTime, lastFrame, lastFrame + 0.02, 1.0, true) == doctest::Approx(playbackTime + 0.02));
	CHECK(Adagio::AnalysisService::ExtrapolatePlayhead(playbackTime, lastFrame, lastFrame + 0.02, 0.5, true) == doctest::Approx(playbackTime + 0.01));
	CHECK(Adagio::AnalysisService::ExtrapolatePlayhead(playbackTime, lastFrame, lastFrame + 0.02, 2.0, true) == doctest::Approx(playbackTime + 0.04));
	// A callback that stopped rather than ran late is clamped, and a stamp from the
	// future contributes nothing.
	CHECK(Adagio::AnalysisService::ExtrapolatePlayhead(playbackTime, lastFrame, lastFrame + 10.0, 1.0, true) == doctest::Approx(playbackTime + 0.25));
	CHECK(Adagio::AnalysisService::ExtrapolatePlayhead(playbackTime, lastFrame, lastFrame - 5.0, 1.0, true) == doctest::Approx(playbackTime));
}

TEST_CASE("A1: a bare sine is named at the octave it sounds")
{
	// Bin spacing is SR/N, not SR/(N/2). Getting it wrong doubled every frequency,
	// which moves the octave digit but not the pitch class - so key and chord output
	// still looked plausible while every note name was an octave high.
	struct PitchCase
	{
		int Midi;
		std::string Name;
	};

	const std::vector<PitchCase> cases = {
		{ 33, "A1" },   // 55 Hz
		{ 60, "C4" },   // 261.63 Hz
		{ 69, "A4" },   // 440 Hz
	};

	for (const PitchCase& testCase : cases)
	{
		CAPTURE(testCase.Name);
		const std::unique_ptr<Adagio::AnalysisResult> result = RunNotePipeline(MakeChordFrame({ testCase.Midi }, 8000, 4096));
		const std::vector<Adagio::Note>& notes = result->Context->Notes;
		REQUIRE_FALSE(notes.empty());

		const Adagio::Note& loudest = LoudestNote(notes);
		CHECK(loudest.PeakInfo.Frequency == doctest::Approx(PitchHz(testCase.Midi)).epsilon(0.01));
		CHECK(loudest.Name == testCase.Name);
	}
}

TEST_CASE("A5: chords are named from a frame of their own notes")
{
	// End to end over the stages that feed ChordPredictor, rather than calling
	// GetChordQuality with an interval set by hand. Every rotation of the set is
	// offered as a candidate, so what is asserted here is that the reading a
	// musician would give is among them. Which one CalculateProbabilities ranks
	// first is a separate question - for A3 C4 E4 G4 it prefers C6 to Am7, and for
	// C4 F4 G4 Bb4 it prefers Gm7 (11) to C7sus4, because any "sus" quality carries
	// a -2.5 penalty and an assumed fifth a +1.0 bonus.
	struct ChordCase
	{
		std::vector<int> Midi;
		std::string Name;
	};

	const std::vector<ChordCase> cases = {
		{ { 60, 64, 67 }, "C" },            // C4 E4 G4
		{ { 57, 60, 64, 67 }, "Am7" },      // A3 C4 E4 G4
		{ { 60, 65, 67, 70 }, "C7sus4" },   // C4 F4 G4 Bb4
	};

	for (const ChordCase& testCase : cases)
	{
		CAPTURE(testCase.Name);
		const std::unique_ptr<Adagio::AnalysisResult> result = RunChordPipeline(MakeChordFrame(testCase.Midi, 8000, 4096));
		REQUIRE(result->Context->Notes.size() == testCase.Midi.size());
		REQUIRE_FALSE(result->Context->PredictedChords.empty());

		bool named = false;
		for (const Adagio::Chord& chord : result->Context->PredictedChords)
		{
			if (chord.Name == testCase.Name)
				named = true;
		}
		CHECK(named);
	}
}

TEST_CASE("A5: a plain major triad is the chord it ranks first")
{
	// The one set above with no second reading: C E G is C, whichever note the
	// ranking starts from.
	const std::unique_ptr<Adagio::AnalysisResult> result = RunChordPipeline(MakeChordFrame({ 60, 64, 67 }, 8000, 4096));
	REQUIRE_FALSE(result->Context->PredictedChords.empty());
	CHECK(result->Context->PredictedChords[0].Name == "C");
	CHECK(result->Context->PredictedChords[0].Root == "C");
}

namespace
{
	// C major for the first second, then the same scale with F# for F, and returns
	// the raw pitch-class accumulator after two seconds at the given hop.
	std::array<double, 12> AccumulateKey(double hopSeconds)
	{
		const int firstScale[] = { 0, 2, 4, 5, 7, 9, 11 };
		const int secondScale[] = { 0, 2, 4, 6, 7, 9, 11 };
		const float weights[] = { 10.0f, 2.0f, 6.0f, 3.0f, 8.0f, 4.0f, 2.0f };

		Adagio::PersistentData persistent;
		Adagio::KeyDetector detector;
		const int frames = static_cast<int>(std::lround(2.0 / hopSeconds));
		for (int k = 1; k <= frames; ++k)
		{
			Adagio::AudioFrame frame;
			frame.Timestamp = k * hopSeconds;
			frame.DeltaTime = hopSeconds;

			Adagio::AnalysisContext context{ frame, &persistent };
			const int* scale = frame.Timestamp <= 1.0 ? firstScale : secondScale;
			for (int i = 0; i < 7; ++i)
				context.Notes.push_back(MakeNote(60 + scale[i], PitchHz(60 + scale[i]), weights[i] / 10.0f, frame.Timestamp));
			detector.Execute(&context);
		}
		return persistent.NoteClassAccumulator;
	}

	// The root presence of the top-ranked chord after holding C E G for a second.
	double HoldTriad(double hopSeconds)
	{
		Adagio::PersistentData persistent;
		Adagio::ChordPredictor predictor;
		double presence = 0.0;
		const int frames = static_cast<int>(std::lround(1.0 / hopSeconds));
		for (int k = 1; k <= frames; ++k)
		{
			Adagio::AudioFrame frame;
			frame.Timestamp = k * hopSeconds;
			frame.DeltaTime = hopSeconds;

			Adagio::AnalysisContext context{ frame, &persistent };
			for (int midi : { 60, 64, 67 })
				context.Notes.push_back(MakeNote(midi, PitchHz(midi), 1.0f, frame.Timestamp));
			predictor.Execute(&context);
			presence = context.PredictedChords.empty() ? 0.0 : context.PredictedChords[0].RootPresenceSeconds;
		}
		return presence;
	}

	struct FrameReading
	{
		std::string Key;
		std::string TopChord;
	};

	// Drives the production pipeline over a signal at a fixed hop, centring each frame
	// on the playhead as AnalysisService does, and records what it reads at each position.
	std::map<int64_t, FrameReading> RunProgression(const std::vector<float>& signal, int hopSamples)
	{
		constexpr uint32_t sampleRate = 8000;
		constexpr uint32_t frameLength = 4096;

		std::unique_ptr<Adagio::AnalysisPipeline> pipeline = Adagio::AnalysisService::CreatePipeline();
		std::map<int64_t, FrameReading> readings;
		for (int64_t position = hopSamples; position < static_cast<int64_t>(signal.size()); position += hopSamples)
		{
			Adagio::AudioFrame frame;
			frame.SampleRate = sampleRate;
			frame.FrameLength = frameLength;
			frame.Timestamp = position / static_cast<double>(sampleRate);
			frame.DeltaTime = hopSamples / static_cast<double>(sampleRate);
			frame.Samples.resize(frameLength);

			const int64_t start = position - frameLength / 2;
			for (uint32_t i = 0; i < frameLength; ++i)
			{
				const int64_t sample = start + i;
				frame.Samples[i] = sample >= 0 && sample < static_cast<int64_t>(signal.size()) ? signal[sample] : 0.0f;
			}

			const std::unique_ptr<Adagio::AnalysisResult> result = pipeline->ProcessFrame(frame);
			const auto& chords = result->Context->PredictedChords;
			// Added notes are left out: they come from classes at SCORE_THRESHOLD, where a
			// finer hop averages over a different set of beat phases and can tip either way.
			readings[position] = { result->Context->PredictedKey.Name, chords.empty() ? std::string() : chords[0].Name.substr(0, chords[0].Name.find(" (")) };
		}
		return readings;
	}
}

TEST_CASE("F1: the key accumulator weighs source time, not frames")
{
	// Counting notes per frame doubled every total when the hop halved. Weighted by
	// the frame's source seconds, hops of 16, 32 and 64 ms hold the same evidence.
	const std::array<double, 12> fine = AccumulateKey(0.016);
	const std::array<double, 12> production = AccumulateKey(HopSeconds);
	const std::array<double, 12> coarse = AccumulateKey(0.064);

	for (int pc = 0; pc < 12; ++pc)
	{
		CAPTURE(pc);
		CHECK(fine[pc] == doctest::Approx(production[pc]).epsilon(0.03));
		CHECK(coarse[pc] == doctest::Approx(production[pc]).epsilon(0.03));
	}
	// Both seconds held F and F# equally, so the more recent F# outweighs the decayed F.
	CHECK(production[5] > 0.0);
	CHECK(production[6] > production[5]);
}

TEST_CASE("F1: a chord root's presence is measured in seconds")
{
	// As a count it grew with the frame rate; as seconds it is the window, whatever the hop.
	const double production = HoldTriad(HopSeconds);
	CHECK(production == doctest::Approx(0.5).epsilon(0.1));
	CHECK(HoldTriad(0.016) == doctest::Approx(production).epsilon(0.05));
	CHECK(HoldTriad(0.064) == doctest::Approx(production).epsilon(0.1));
}

TEST_CASE("F1: the production pipeline reads the same key and chords at every speed")
{
	// The hop is fixed in wall time, so 0.5x, 1x and 2x analyse the source every 128,
	// 256 and 512 samples. The first test to run HPS: it goes through CreatePipeline.
	constexpr uint32_t sampleRate = 8000;
	const std::vector<std::vector<int>> progression = {
		{ 60, 64, 67 },   // C
		{ 65, 69, 72 },   // F
		{ 67, 71, 74 },   // G
		{ 60, 64, 67 },   // C
	};
	const size_t segmentSamples = sampleRate * 5 / 2;

	std::vector<float> signal(segmentSamples * progression.size());
	for (size_t i = 0; i < signal.size(); ++i)
	{
		const float t = static_cast<float>(i) / static_cast<float>(sampleRate);
		float sample = 0.0f;
		for (int midi : progression[i / segmentSamples])
		{
			for (int harmonic = 1; harmonic <= 4; ++harmonic)
				sample += std::sin(2.0f * Pi * PitchHz(midi) * harmonic * t) / static_cast<float>(harmonic);
		}
		signal[i] = sample;
	}

	const std::map<int64_t, FrameReading> halfSpeed = RunProgression(signal, 128);
	const std::map<int64_t, FrameReading> fullSpeed = RunProgression(signal, 256);
	const std::map<int64_t, FrameReading> doubleSpeed = RunProgression(signal, 512);

	// Positions common to all three hops, each at least half a second from a chord change.
	for (int64_t position : { 15360, 35840, 51200, 71680, 79872 })
	{
		CAPTURE(position);
		REQUIRE(fullSpeed.count(position) == 1);
		const FrameReading& expected = fullSpeed.at(position);
		CAPTURE(expected.Key);
		CAPTURE(expected.TopChord);
		CHECK_FALSE(expected.TopChord.empty());
		CHECK(halfSpeed.at(position).Key == expected.Key);
		CHECK(halfSpeed.at(position).TopChord == expected.TopChord);
		CHECK(doubleSpeed.at(position).Key == expected.Key);
		CHECK(doubleSpeed.at(position).TopChord == expected.TopChord);
	}
}

namespace
{
	// Runs NoteDetector over frames holding a peak at each frequency for the given source
	// seconds, and returns the tuning error the last frame reported.
	std::optional<float> HoldTuning(Adagio::NoteDetector& detector, Adagio::PersistentData& persistent, const std::vector<float>& frequencies, double seconds, double hopSeconds = HopSeconds)
	{
		std::optional<float> reading;
		const int frames = (int)std::lround(seconds / hopSeconds);
		for (int k = 0; k < frames; ++k)
		{
			Adagio::AudioFrame frame;
			frame.DeltaTime = hopSeconds;

			Adagio::AnalysisContext context{ frame, &persistent };
			for (float hz : frequencies)
				context.Peaks.push_back({ hz, 1.0f, 1.0f });
			detector.Execute(&context);
			reading = context.AvgTuningError;
		}
		return reading;
	}

	// A second of one note 10 cents sharp, then a second of two notes 10 cents flat.
	float SharpThenFlat(double hopSeconds)
	{
		Adagio::NoteDetector detector;
		Adagio::PersistentData persistent;
		HoldTuning(detector, persistent, { PitchHz(69, 10.0f) }, 1.0, hopSeconds);
		return HoldTuning(detector, persistent, { PitchHz(69, -10.0f), PitchHz(76, -10.0f) }, 1.0, hopSeconds).value();
	}
}

TEST_CASE("the tuning error is a mean over notes, and a rest holds it rather than pulling it to zero")
{
	Adagio::NoteDetector detector;
	Adagio::PersistentData persistent;
	CHECK_FALSE(HoldTuning(detector, persistent, {}, 0.5).has_value());

	// Per note, not per frame: one note 12 cents sharp and two in tune are 4 cents sharp.
	const std::optional<float> chord = HoldTuning(detector, persistent, { PitchHz(60, 12.0f), PitchHz(64), PitchHz(67) }, 2.0);
	REQUIRE(chord.has_value());
	CHECK(*chord == doctest::Approx(4.0f).epsilon(0.01));

	const std::optional<float> rest = HoldTuning(detector, persistent, {}, 0.5);
	REQUIRE(rest.has_value());
	CHECK(*rest == doctest::Approx(4.0f).epsilon(0.01));

	// Long enough that the window holds next to nothing: no reading, not a stale one.
	CHECK_FALSE(HoldTuning(detector, persistent, {}, 2.0).has_value());
}

TEST_CASE("the tuning error weighs source time, not frames")
{
	const float production = SharpThenFlat(HopSeconds);
	CHECK(production < -8.0f);
	CHECK(production > -10.0f);
	CHECK(SharpThenFlat(0.016) == doctest::Approx(production).epsilon(0.03));
	CHECK(SharpThenFlat(0.064) == doctest::Approx(production).epsilon(0.03));
}

TEST_CASE("a new A4 reference moves the tuning error at once")
{
	// The UI analyses a single paused frame after a change, which is one hop of evidence.
	Adagio::NoteDetector detector;
	Adagio::PersistentData persistent;
	HoldTuning(detector, persistent, { PitchHz(69), PitchHz(72) }, 2.0);

	detector.ApplySettings({ { "A440_TUNING", 442.0 } });
	const std::optional<float> retuned = HoldTuning(detector, persistent, { PitchHz(69), PitchHz(72) }, HopSeconds);
	REQUIRE(retuned.has_value());
	CHECK(*retuned == doctest::Approx(-1200.0 * std::log2(442.0 / 440.0)).epsilon(0.01));
}

TEST_CASE("the analysis event sends the tuning error, or null with none")
{
	Adagio::AudioFrame frame;
	Adagio::AnalysisResult result;
	result.Context = std::make_unique<Adagio::AnalysisContext>(frame);
	CHECK(Adagio::AnalysisPipeline::GetResultJson(result).at("value").at("avgTuningError").is_null());

	result.Context->AvgTuningError = -3.5f;
	CHECK(Adagio::AnalysisPipeline::GetResultJson(result).at("value").at("avgTuningError").get<double>() == doctest::Approx(-3.5));
}

// A10 and the Note Selection plan. HPS gives every partial an image a twelfth below
// it, and PeakExtractor reports images and partials alike as notes. The plan replaces
// both with PartialExtractor, HarmonicGrouper and NoteSelector, behind SpectrumFilter.
namespace
{
	constexpr double PiD = 3.14159265358979323846;
	constexpr uint32_t AnalysisRate = 8000;
	constexpr int AnalysisHop = 256;

	struct Component
	{
		double Frequency = 0.0;
		float Amplitude = 0.0f;
	};

	// Partials 1 to count of one note at the amplitudes the shape gives them, stretched
	// as a piano's are when inharmonicity is set. Partials at or above Nyquist are left
	// out, as Preprocess's lowpass leaves them out of the analysis stream.
	std::vector<Component> Tone(double fundamentalHz, int partials, const std::function<float(int)>& amplitude, double inharmonicity = 0.0)
	{
		std::vector<Component> components;
		for (int k = 1; k <= partials; ++k)
		{
			const double hz = k * fundamentalHz * std::sqrt(1.0 + inharmonicity * k * k);
			if (hz >= AnalysisRate / 2.0)
				break;
			if (amplitude(k) > 0.0f)
				components.push_back({ hz, amplitude(k) });
		}
		return components;
	}

	// Partials at 1/k amplitude, the shape the plan measured with.
	std::vector<Component> HarmonicTone(double fundamentalHz, int partials, float gain = 1.0f)
	{
		return Tone(fundamentalHz, partials, [gain](int k) { return gain / static_cast<float>(k); });
	}

	std::vector<Component> Mix(std::initializer_list<std::vector<Component>> voices)
	{
		std::vector<Component> components;
		for (const std::vector<Component>& voice : voices)
			components.insert(components.end(), voice.begin(), voice.end());
		return components;
	}

	// The components summed over the given seconds, in double precision so a 4 s tone
	// keeps its phase, with uniform noise snrDb below the tone from a fixed seed. Then
	// the whole is scaled by gainDb, noise included, so only the level changes.
	std::vector<float> Synthesize(const std::vector<Component>& components, double seconds, double snrDb = 60.0, double gainDb = 0.0)
	{
		const size_t length = static_cast<size_t>(seconds * AnalysisRate);
		std::vector<double> mixed(length, 0.0);
		for (const Component& component : components)
		{
			const double step = 2.0 * PiD * component.Frequency / AnalysisRate;
			for (size_t i = 0; i < length; ++i)
				mixed[i] += component.Amplitude * std::sin(step * static_cast<double>(i));
		}

		double power = 0.0;
		for (double sample : mixed)
			power += sample * sample;
		// Uniform noise on [-1, 1) has an RMS of 1/sqrt(3).
		const double noiseScale = std::sqrt(3.0 * power / static_cast<double>(length)) * std::pow(10.0, -snrDb / 20.0);
		const double gain = std::pow(10.0, gainDb / 20.0);

		uint32_t state = 0x9E3779B9u;
		std::vector<float> signal(length);
		for (size_t i = 0; i < length; ++i)
		{
			state ^= state << 13;
			state ^= state >> 17;
			state ^= state << 5;
			const double noise = state / 2147483648.0 - 1.0;
			signal[i] = static_cast<float>(gain * (mixed[i] + noiseScale * noise));
		}
		return signal;
	}

	// The frame centred on a sample, as AnalysisService centres one on the playhead.
	Adagio::AudioFrame FrameAt(const std::vector<float>& signal, size_t centre, uint32_t frameLength)
	{
		Adagio::AudioFrame frame;
		frame.SampleRate = AnalysisRate;
		frame.FrameLength = frameLength;
		frame.Timestamp = centre / static_cast<double>(AnalysisRate);
		frame.DeltaTime = AnalysisHop / static_cast<double>(AnalysisRate);
		frame.Samples.resize(frameLength);
		for (uint32_t i = 0; i < frameLength; ++i)
			frame.Samples[i] = signal[centre - frameLength / 2 + i];
		return frame;
	}

	// The notes of every frame that lies wholly inside the signal, a production hop apart.
	std::vector<std::vector<Adagio::Note>> DetectNotes(Adagio::AnalysisPipeline& pipeline, const std::vector<float>& signal, uint32_t frameLength = 4096)
	{
		std::vector<std::vector<Adagio::Note>> frames;
		for (size_t centre = frameLength / 2; centre + frameLength / 2 <= signal.size(); centre += AnalysisHop)
			frames.push_back(pipeline.ProcessFrame(FrameAt(signal, centre, frameLength))->Context->Notes);
		return frames;
	}

	bool HasNote(const std::vector<Adagio::Note>& notes, int midi)
	{
		for (const Adagio::Note& note : notes)
		{
			if (note.Midi == midi)
				return true;
		}
		return false;
	}

	// The share of each pitch class in the score-weighted note weight of every frame,
	// which is what the key and chord histograms accumulate.
	std::array<double, 12> PitchClassShare(const std::vector<std::vector<Adagio::Note>>& frames)
	{
		std::array<double, 12> share{};
		double total = 0.0;
		for (const std::vector<Adagio::Note>& notes : frames)
		{
			for (const Adagio::Note& note : notes)
			{
				share[note.Midi % 12] += note.PeakInfo.Score;
				total += note.PeakInfo.Score;
			}
		}
		for (double& value : share)
			value = total > 0.0 ? value / total : 0.0;
		return share;
	}
}

TEST_CASE("A10: a single tone gives one note per frame")
{
	// HPS and PeakExtractor gave a C4 sine 3 notes a frame, 29% of its weight on F, and a
	// C3 with 8 harmonics 11. Only the spectrum's own peaks can be notes now, and each
	// note explains its harmonics away before the next is picked. The level and the
	// frame length must not matter. A note without its fundamental is named by its
	// lowest partial, an octave up; the plan lists that among its risks.
	const double c2 = PitchHz(36);
	const double c3 = PitchHz(48);

	struct ToneCase
	{
		std::string Name;
		std::vector<Component> Components;
		int Midi;
	};

	const std::vector<ToneCase> cases = {
		{ "C4, pure sine", HarmonicTone(PitchHz(60), 1), 60 },
		{ "C3, 8 harmonics", HarmonicTone(c3, 8), 48 },
		{ "C2, 12 harmonics", HarmonicTone(c2, 12), 36 },
		{ "C3, odd harmonics only", Tone(c3, 8, [](int k) { return k % 2 == 1 ? 1.0f / k : 0.0f; }), 48 },
		{ "C3 without its fundamental", Tone(c3, 8, [](int k) { return k > 1 ? 1.0f / k : 0.0f; }), 60 },
		{ "C6, 4 harmonics", HarmonicTone(PitchHz(84), 4), 84 },
		{ "C2, piano-like, 16 partials", Tone(c2, 16, [](int k) { return 1.0f / k; }, 1e-4), 36 },
	};

	for (const ToneCase& testCase : cases)
	{
		for (double gainDb : { 0.0, -40.0 })
		{
			for (uint32_t frameLength : { 2048u, 4096u, 8192u })
			{
				CAPTURE(testCase.Name);
				CAPTURE(gainDb);
				CAPTURE(frameLength);
				std::unique_ptr<Adagio::AnalysisPipeline> pipeline = Adagio::AnalysisService::CreatePipeline();
				const auto frames = DetectNotes(*pipeline, Synthesize(testCase.Components, 4.0, 60.0, gainDb), frameLength);
				REQUIRE_FALSE(frames.empty());

				size_t wrong = 0;
				for (const std::vector<Adagio::Note>& notes : frames)
				{
					if (notes.size() != 1 || notes[0].Midi != testCase.Midi)
						++wrong;
				}
				CHECK(wrong == 0);
			}
		}
	}
}

TEST_CASE("A10: a C3 whose 2nd partial is 6 dB above its fundamental is one note" * doctest::should_fail())
{
	// The calibrated defaults keep the C3 but add its 2nd partial as a C4 beside it, in
	// nearly every frame of 4,096 and 8,192 samples and in few of 2,048. Accepted with
	// the calibration on real tracks; the decoration goes when a change fixes it.
	const std::vector<Component> tone = Tone(PitchHz(48), 8, [](int k) { return k == 2 ? 2.0f : 1.0f / k; });

	for (double gainDb : { 0.0, -40.0 })
	{
		for (uint32_t frameLength : { 2048u, 4096u, 8192u })
		{
			CAPTURE(gainDb);
			CAPTURE(frameLength);
			std::unique_ptr<Adagio::AnalysisPipeline> pipeline = Adagio::AnalysisService::CreatePipeline();
			const auto frames = DetectNotes(*pipeline, Synthesize(tone, 4.0, 60.0, gainDb), frameLength);
			REQUIRE_FALSE(frames.empty());

			size_t wrong = 0;
			for (const std::vector<Adagio::Note>& notes : frames)
			{
				if (notes.size() != 1 || notes[0].Midi != 48)
					++wrong;
			}
			CHECK(wrong == 0);
		}
	}
}

TEST_CASE("A10: two notes sharing partials are both found in every frame")
{
	// HPS and PeakExtractor found both because they reported nearly every partial. The
	// new stages must keep both while explaining harmonics away: G4 sits exactly on C3's
	// 3rd partial and C4 on its 2nd, so each survives by what rises above C3's envelope
	// there. E4's 2nd partial merges with C3's 5th, 14 cents away, and C3's series takes
	// its 4th as C3's 10th, so its salience falls further below C3's than its level does:
	// at -20 dB it crosses NoteSelector's STOP_DB of 30 in some frames, at -18 dB in none.
	const double c3 = PitchHz(48);

	struct PairCase
	{
		std::string Name;
		std::vector<Component> Components;
		int Low;
		int High;
	};

	const std::vector<PairCase> cases = {
		{ "C3 + G4 on its 3rd partial", Mix({ HarmonicTone(c3, 8), HarmonicTone(3.0 * c3, 8) }), 48, 67 },
		{ "C3 + C4", Mix({ HarmonicTone(c3, 8), HarmonicTone(2.0 * c3, 8) }), 48, 60 },
		{ "C3 + E4 at -15 dB", Mix({ HarmonicTone(c3, 8), HarmonicTone(PitchHz(64), 8, 0.178f) }), 48, 64 },
	};

	for (const PairCase& testCase : cases)
	{
		CAPTURE(testCase.Name);
		std::unique_ptr<Adagio::AnalysisPipeline> pipeline = Adagio::AnalysisService::CreatePipeline();
		const auto frames = DetectNotes(*pipeline, Synthesize(testCase.Components, 4.0));
		REQUIRE_FALSE(frames.empty());

		size_t missing = 0;
		for (const std::vector<Adagio::Note>& notes : frames)
		{
			if (!HasNote(notes, testCase.Low) || !HasNote(notes, testCase.High))
				++missing;
		}
		CHECK(missing == 0);
	}
}

TEST_CASE("A10: two bass notes a semitone apart both come through")
{
	// 65 and 69 Hz are two bins apart at a frame of 4,096, inside the peak test's
	// +-3 bins, so the fundamentals make one peak and the upper partials have to tell
	// the notes apart. At the limit of the frame, so it asks for the pitch classes
	// and an even split, not both notes in every frame. HPS's images split it 0.36 to
	// 0.64; the new stages split it 0.49 to 0.51.
	std::unique_ptr<Adagio::AnalysisPipeline> pipeline = Adagio::AnalysisService::CreatePipeline();
	const auto frames = DetectNotes(*pipeline, Synthesize(Mix({ HarmonicTone(PitchHz(36), 12), HarmonicTone(PitchHz(37), 12) }), 4.0));
	const std::array<double, 12> share = PitchClassShare(frames);

	CAPTURE(share[0]);
	CAPTURE(share[1]);
	REQUIRE(share[0] + share[1] > 0.0);
	CHECK(share[0] / (share[0] + share[1]) > 0.4);
	CHECK(share[1] / (share[0] + share[1]) > 0.4);
}

TEST_CASE("A10: a triad in noise gives its three notes in every frame")
{
	// C3-E3-G3 with 8 harmonics each, at 10 dB SNR. HPS and PeakExtractor gave 14 notes a
	// frame from it, adding their octaves and an image at C2.
	const std::vector<Component> triad = Mix({ HarmonicTone(PitchHz(48), 8), HarmonicTone(PitchHz(52), 8), HarmonicTone(PitchHz(55), 8) });
	std::unique_ptr<Adagio::AnalysisPipeline> pipeline = Adagio::AnalysisService::CreatePipeline();
	const auto frames = DetectNotes(*pipeline, Synthesize(triad, 4.0, 10.0));
	REQUIRE_FALSE(frames.empty());

	size_t missing = 0;
	for (const std::vector<Adagio::Note>& notes : frames)
	{
		if (!HasNote(notes, 48) || !HasNote(notes, 52) || !HasNote(notes, 55))
			++missing;
	}
	CHECK(missing == 0);
}

TEST_CASE("A10: a triad in noise gives no other notes" * doctest::should_fail())
{
	// The same triad. PartialExtractor's calibrated gate of 4 dB lets the noise's own
	// peaks through, and about two a frame become notes, from F4 up to B6. Accepted with
	// the calibration on real tracks; the decoration goes when a change fixes it.
	const std::vector<Component> triad = Mix({ HarmonicTone(PitchHz(48), 8), HarmonicTone(PitchHz(52), 8), HarmonicTone(PitchHz(55), 8) });
	std::unique_ptr<Adagio::AnalysisPipeline> pipeline = Adagio::AnalysisService::CreatePipeline();
	const auto frames = DetectNotes(*pipeline, Synthesize(triad, 4.0, 10.0));
	REQUIRE_FALSE(frames.empty());

	size_t extra = 0;
	for (const std::vector<Adagio::Note>& notes : frames)
	{
		if (notes.size() != 3)
			++extra;
	}
	CHECK(extra == 0);
}

TEST_CASE("A10: the spectrum leaves the pipeline as SpectrumFilter left it" * doctest::should_fail())
{
	// SpectrumFilter is the EQ for every stage after it, and none of them writes
	// Magnitudes, so the screen shows the FFT after the EQ. Today it shows HPS's product.
	const std::vector<float> signal = Synthesize(Mix({ HarmonicTone(PitchHz(48), 8), HarmonicTone(PitchHz(52), 8), HarmonicTone(PitchHz(55), 8) }), 1.0);
	const Adagio::AudioFrame frame = FrameAt(signal, signal.size() / 2, 4096);

	Adagio::AnalysisPipeline equaliser;
	equaliser.AddStage(std::make_unique<Adagio::FFTProcessor>());
	equaliser.AddStage(std::make_unique<Adagio::SpectrumFilterProcessor>());
	const std::unique_ptr<Adagio::AnalysisResult> expected = equaliser.ProcessFrame(frame);
	const std::unique_ptr<Adagio::AnalysisResult> actual = Adagio::AnalysisService::CreatePipeline()->ProcessFrame(frame);

	const kfr::univector<float>& want = expected->Context->Magnitudes;
	const kfr::univector<float>& got = actual->Context->Magnitudes;
	CHECK(actual->Context->BinHz == expected->Context->BinHz);
	REQUIRE(got.size() == want.size());
	size_t changed = 0;
	for (size_t i = 0; i < want.size(); ++i)
	{
		if (got[i] != want[i])
			++changed;
	}
	CHECK(changed == 0);
}

TEST_CASE("A10: no note is reported below SpectrumFilter's low cut")
{
	// A-flat 1 is 51.9 Hz: above NoteSelector's MIN_FREQ but below the EQ's low cut, so
	// with the EQ in front its fundamental never reaches the stages. The note is still
	// heard through its harmonics, an octave up.
	const float lowCut = Adagio::SpectrumFilterProcessorSettings{}.LOW_CUT_C1;
	REQUIRE(PitchHz(32) < lowCut);

	std::unique_ptr<Adagio::AnalysisPipeline> pipeline = Adagio::AnalysisService::CreatePipeline();
	const auto frames = DetectNotes(*pipeline, Synthesize(HarmonicTone(PitchHz(32), 8), 4.0));

	size_t below = 0;
	size_t heard = 0;
	for (const std::vector<Adagio::Note>& notes : frames)
	{
		bool flat = false;
		for (const Adagio::Note& note : notes)
		{
			if (note.PeakInfo.Frequency < lowCut)
				++below;
			if (note.Midi % 12 == 8)
				flat = true;
		}
		if (flat)
			++heard;
	}
	CHECK(below == 0);
	CHECK(heard == frames.size());
}

TEST_CASE("A10: the production pipeline reads a C-F-G-C progression in C major")
{
	// Each partial's image a twelfth below lands a fifth lower in pitch class, so the
	// roots' ghosts are F, B-flat and C, pulling the key toward F. It holds today, and
	// has to hold once every weight comes from the chord tones alone.
	const std::vector<std::vector<int>> progression = {
		{ 60, 64, 67 },   // C
		{ 65, 69, 72 },   // F
		{ 67, 71, 74 },   // G
		{ 60, 64, 67 },   // C
	};
	const size_t segmentSamples = AnalysisRate * 5 / 2;

	std::vector<float> signal;
	for (const std::vector<int>& chord : progression)
	{
		std::vector<Component> components;
		for (int midi : chord)
		{
			const std::vector<Component> tone = HarmonicTone(PitchHz(midi), 6);
			components.insert(components.end(), tone.begin(), tone.end());
		}
		const std::vector<float> segment = Synthesize(components, segmentSamples / static_cast<double>(AnalysisRate));
		signal.insert(signal.end(), segment.begin(), segment.end());
	}

	std::unique_ptr<Adagio::AnalysisPipeline> pipeline = Adagio::AnalysisService::CreatePipeline();
	std::string key;
	for (size_t centre = 2048; centre + 2048 <= signal.size(); centre += AnalysisHop)
		key = pipeline->ProcessFrame(FrameAt(signal, centre, 4096))->Context->PredictedKey.Name;
	CHECK(key == "C Major");
}

#ifdef ADAGIO_NOTE_SELECTION
namespace
{
	// The new stages in production order, without the notes, key and chords after them.
	std::unique_ptr<Adagio::AnalysisResult> RunNoteSelection(const Adagio::AudioFrame& frame)
	{
		Adagio::AnalysisPipeline pipeline;
		pipeline.AddStage(std::make_unique<Adagio::FFTProcessor>());
		pipeline.AddStage(std::make_unique<Adagio::SpectrumFilterProcessor>());
		pipeline.AddStage(std::make_unique<Adagio::PartialExtractor>());
		pipeline.AddStage(std::make_unique<Adagio::HarmonicGrouper>());
		pipeline.AddStage(std::make_unique<Adagio::NoteSelector>());
		return pipeline.ProcessFrame(frame);
	}

	// The index of the partial nearest a frequency, or -1 with none.
	int NearestPartial(const std::vector<Adagio::Partial>& partials, double hz)
	{
		int nearest = -1;
		for (size_t i = 0; i < partials.size(); ++i)
		{
			if (nearest < 0 || std::abs(partials[i].Frequency - hz) < std::abs(partials[nearest].Frequency - hz))
				nearest = static_cast<int>(i);
		}
		return nearest;
	}

	double ToDb(double ratio)
	{
		return 20.0 * std::log10(ratio);
	}
}

TEST_CASE("A10: PartialExtractor finds one partial per harmonic and no sidelobes")
{
	// A bin must be the largest within +-3 bins, which a sidelobe never is beside its
	// main lobe, and stand GATE_DB above the median of the 25 bins around it. Each
	// partial's frequency is the parabola's vertex: up to 0.19 bins from the nearest bin
	// here, which rounding to the bin would miss. The default gate of 4 dB also lets the
	// noise floor's own bumps through, above the 8th partial and 79 dB or more below the
	// fundamental here, so anything but a harmonic has to be 60 dB down.
	const double c3 = PitchHz(48);
	const std::vector<float> signal = Synthesize(HarmonicTone(c3, 8), 1.0);
	const std::unique_ptr<Adagio::AnalysisResult> result = RunNoteSelection(FrameAt(signal, signal.size() / 2, 4096));
	const std::vector<Adagio::Partial>& partials = result->Context->Partials;
	const float binHz = result->Context->BinHz;

	const int fundamental = NearestPartial(partials, c3);
	REQUIRE(fundamental >= 0);
	std::vector<bool> harmonic(partials.size(), false);
	for (int k = 1; k <= 8; ++k)
	{
		CAPTURE(k);
		const int index = NearestPartial(partials, k * c3);
		harmonic[index] = true;
		const Adagio::Partial& partial = partials[index];
		CHECK(std::abs(partial.Frequency - k * c3) < 0.1 * binHz);
		CHECK(partial.BinIndex == static_cast<int>(std::lround(k * c3 / binHz)));
		CHECK(partial.Magnitude == result->Context->Magnitudes[partial.BinIndex]);
	}
	for (size_t i = 0; i < partials.size(); ++i)
	{
		if (harmonic[i])
			continue;
		CAPTURE(partials[i].Frequency);
		CHECK(ToDb(partials[i].Magnitude / partials[fundamental].Magnitude) < -60.0);
	}
	CHECK(result->Context->NoiseFloor > 0.0f);
	CHECK(result->Context->GateLevel > result->Context->NoiseFloor);
}

TEST_CASE("A10: HarmonicGrouper gives C3 a series holding its partials 2 to 8 in order")
{
	// Entry k - 1 of a partial's series is the partial nearest k times its frequency
	// within MATCH_CENTS, or -1, for every multiple below Nyquist. The first entry is the
	// partial itself. Past the 8th an entry is -1 or one of the noise floor's bumps that
	// PartialExtractor's 4 dB gate lets through, 60 dB or more below the fundamental.
	const double c3 = PitchHz(48);
	const std::vector<float> signal = Synthesize(HarmonicTone(c3, 8), 1.0);
	const std::unique_ptr<Adagio::AnalysisResult> result = RunNoteSelection(FrameAt(signal, signal.size() / 2, 4096));
	const std::vector<Adagio::Partial>& partials = result->Context->Partials;
	REQUIRE(result->Context->HarmonicGroups.size() == partials.size());

	const int fundamental = NearestPartial(partials, c3);
	REQUIRE(fundamental >= 0);
	const std::vector<int>& series = result->Context->HarmonicGroups[fundamental];
	// 30 x C3 is the last multiple below 4 kHz.
	REQUIRE(series.size() == 30);
	CHECK(series[0] == fundamental);
	for (int k = 2; k <= 8; ++k)
	{
		CAPTURE(k);
		REQUIRE(series[k - 1] >= 0);
		CHECK(series[k - 1] == NearestPartial(partials, k * c3));
		CHECK(partials[series[k - 1]].Frequency > partials[series[k - 2]].Frequency);
	}
	for (size_t k = 9; k <= series.size(); ++k)
	{
		CAPTURE(k);
		if (series[k - 1] != -1)
			CHECK(ToDb(partials[series[k - 1]].Magnitude / partials[fundamental].Magnitude) < -60.0);
	}
}

TEST_CASE("A10: NoteSelector's salience is the plan's worked example")
{
	// Salience is the geometric mean of what remains of a partial's partials 1 to
	// HARMONICS + 1, a missing one counting 30 dB below it. With partials at 1/k and the
	// default HARMONICS of 3, C3 is the mean of 0, -6, -9.5 and -12 dB; its 2nd partial,
	// C4, of C3's 2nd, 4th, 6th and 8th; its 3rd, G4, of C3's 3rd, 6th, 9th and 12th, so
	// the tone has 12 partials rather than 8. A pure sine is (0 - 30 - 30 - 30) / 4 dB
	// below its level: the noise floor's bumps its series may reach are lower than that.
	// FindSalience(context, partial, residuals) is NoteSelector's own, given each partial
	// whole, as before any pick.
	const double c3 = PitchHz(48);
	Adagio::NoteSelector selector;

	{
		const std::vector<float> signal = Synthesize(HarmonicTone(c3, 12), 1.0);
		const std::unique_ptr<Adagio::AnalysisResult> result = RunNoteSelection(FrameAt(signal, signal.size() / 2, 4096));
		Adagio::AnalysisContext& context = *result->Context;

		std::vector<float> residuals;
		for (const Adagio::Partial& partial : context.Partials)
			residuals.push_back(partial.Magnitude);

		const int fundamental = NearestPartial(context.Partials, c3);
		REQUIRE(fundamental >= 0);
		const double reference = context.Partials[fundamental].Magnitude;
		const auto salienceDb = [&](double hz)
		{
			return ToDb(selector.FindSalience(&context, NearestPartial(context.Partials, hz), residuals) / reference);
		};

		CHECK(std::abs(salienceDb(c3) + 6.9) <= 1.0);
		CHECK(std::abs(salienceDb(2.0 * c3) + 12.9) <= 1.0);
		CHECK(std::abs(salienceDb(3.0 * c3) + 16.4) <= 1.0);
	}

	{
		const std::vector<float> signal = Synthesize(HarmonicTone(PitchHz(60), 1), 1.0);
		const std::unique_ptr<Adagio::AnalysisResult> result = RunNoteSelection(FrameAt(signal, signal.size() / 2, 4096));
		Adagio::AnalysisContext& context = *result->Context;

		std::vector<float> residuals;
		for (const Adagio::Partial& partial : context.Partials)
			residuals.push_back(partial.Magnitude);

		const int sine = NearestPartial(context.Partials, PitchHz(60));
		REQUIRE(sine >= 0);
		CHECK(std::abs(ToDb(selector.FindSalience(&context, sine, residuals) / context.Partials[sine].Magnitude) + 22.5) <= 1.0);
	}
}
#endif
