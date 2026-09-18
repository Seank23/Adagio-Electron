// Regression tests for the analysis findings in the architecture review.
//
// The cases marked doctest::should_fail reproduce bugs that Phase 2 fixes. They
// are decorated rather than left plainly failing so `ctest` can gate merges from
// Phase 0 onward. When Phase 2 lands the fix, the decoration itself starts
// failing, which is the signal to delete it and keep the assertion.
#include "../src/Analysis/AnalysisPipeline.h"
#include "../src/Analysis/AnalysisService.h"
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
	// shape rather than a bare sine.
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

	// Pure sines at the given pitches, with no harmonics to be mistaken for chord
	// tones of their own.
	Adagio::AudioFrame MakeChordFrame(const std::vector<int>& midiNotes, uint32_t sampleRate, uint32_t frameLength)
	{
		Adagio::AudioFrame frame;
		frame.SampleRate = sampleRate;
		frame.FrameLength = frameLength;
		frame.Timestamp = 1.0;
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

TEST_CASE("A6: rolling notes expire on absolute distance from the frame")
{
	// After a backwards seek the stored notes sit in the future, where the old
	// signed compare (timestamp - note.Timestamp > window) is negative, so they
	// never expired and kept steering the key until playback caught up.
	auto expire = [](Adagio::PersistentData& persistent, double frameTimestamp)
	{
		Adagio::AudioFrame frame;
		frame.SampleRate = 8000;
		frame.FrameLength = 4096;
		frame.Timestamp = frameTimestamp;

		Adagio::AnalysisContext context{ frame, &persistent };
		context.Settings = nlohmann::json::object();

		Adagio::NoteDetector detector;
		detector.Execute(&context);
	};

	// NoteDetector's ROLLING_WINDOW defaults to 20 s.
	Adagio::PersistentData afterSeek;
	afterSeek.RollingNotes.push_back(MakeNote(60, PitchHz(60), 1.0f, 40.0));
	afterSeek.RollingNotes.push_back(MakeNote(64, PitchHz(64), 1.0f, 41.0));
	expire(afterSeek, 1.0);
	CHECK(afterSeek.RollingNotes.empty());

	// A seek shorter than the window leaves the notes it lands among alone.
	Adagio::PersistentData withinWindow;
	withinWindow.RollingNotes.push_back(MakeNote(60, PitchHz(60), 1.0f, 0.5));
	withinWindow.RollingNotes.push_back(MakeNote(64, PitchHz(64), 1.0f, 15.0));
	expire(withinWindow, 1.0);
	CHECK(withinWindow.RollingNotes.size() == 2);
}

TEST_CASE("A6: the chord window reaches the oldest note in range")
{
	// The reverse walk compared an unsigned index with >= 0, which never ends: once
	// every note was inside the window it ran off the front of the deque.
	Adagio::AudioFrame frame;
	frame.SampleRate = 8000;
	frame.FrameLength = 4096;
	frame.Timestamp = 1.0;

	// ChordPredictor's ROLLING_WINDOW defaults to 0.5 s, so all three are in range.
	Adagio::PersistentData persistent;
	persistent.RollingNotes.push_back(MakeNote(60, PitchHz(60), 3.0f, 0.9));
	persistent.RollingNotes.push_back(MakeNote(64, PitchHz(64), 2.0f, 0.95));
	persistent.RollingNotes.push_back(MakeNote(67, PitchHz(67), 1.0f, 1.0));

	Adagio::AnalysisContext context{ frame, &persistent };
	context.Settings = nlohmann::json::object();

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
