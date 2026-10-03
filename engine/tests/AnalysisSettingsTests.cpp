// The analysis settings channel: the schema a client generates its form from, and
// the validation a value is put through before it can reach a stage.
//
// Every rule here comes from the stage's own GetSettings(), so a new stage is
// covered the moment it is registered.
#include "../src/Analysis/AnalysisPipeline.h"
#include "../src/Analysis/AnalysisService.h"
#include "../src/Analysis/FFTProcessor.h"
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

	std::unique_ptr<Adagio::AnalysisPipeline> MakePipeline()
	{
		auto pipeline = std::make_unique<Adagio::AnalysisPipeline>();
		pipeline->AddStage(std::make_unique<Adagio::FFTProcessor>());
		pipeline->AddStage(std::make_unique<Adagio::PeakExtractor>());
		pipeline->AddStage(std::make_unique<Adagio::NoteDetector>());
		return pipeline;
	}

	// By value, not by pointer: the schema is usually a temporary at the call site, and
	// a pointer into it would dangle the moment the statement ended.
	nlohmann::json FindSetting(const nlohmann::json& schema, const std::string& stage, const std::string& key)
	{
		for (const auto& stageJson : schema.at("stages"))
		{
			if (stageJson.at("name") != stage)
				continue;
			for (const auto& setting : stageJson.at("settings"))
			{
				if (setting.at("key") == key)
					return setting;
			}
		}
		return {};
	}
}

TEST_CASE("The schema names every stage in pipeline order, with the value in force")
{
	const nlohmann::json schema = MakePipeline()->GetSchemaJson();

	REQUIRE(schema.at("stages").size() == 3);
	CHECK(schema.at("stages")[0].at("name") == "FFTProcessor");
	CHECK(schema.at("stages")[1].at("name") == "PeakExtractor");
	CHECK(schema.at("stages")[2].at("name") == "NoteDetector");

	const nlohmann::json window = FindSetting(schema, "FFTProcessor", "WINDOW");
	REQUIRE_FALSE(window.is_null());
	CHECK(window.at("type") == "enum");
	// The default is what the stage declares, and the value is what it is running with.
	CHECK(window.at("default") == "Hamming");
	CHECK(window.at("value") == "Hamming");
	CHECK(window.at("options").size() == 4);
}

TEST_CASE("A setting is checked against the stage's own schema")
{
	std::unique_ptr<Adagio::AnalysisPipeline> pipeline = MakePipeline();
	std::string error;

	CHECK_FALSE(pipeline->SetSetting("NoSuchStage", "WINDOW", "Hann", error));
	CHECK(error.find("NoSuchStage") != std::string::npos);

	CHECK_FALSE(pipeline->SetSetting("FFTProcessor", "NO_SUCH_KEY", 1, error));
	CHECK(error.find("NO_SUCH_KEY") != std::string::npos);

	// An enum takes one of its own options and nothing else.
	CHECK_FALSE(pipeline->SetSetting("FFTProcessor", "WINDOW", "Gaussian", error));
	CHECK_FALSE(pipeline->SetSetting("FFTProcessor", "WINDOW", 3, error));
	CHECK(pipeline->SetSetting("FFTProcessor", "WINDOW", "Hann", error));

	// MAX_PEAKS is declared an int, so 4.5 is not a value it can hold.
	CHECK_FALSE(pipeline->SetSetting("PeakExtractor", "MAX_PEAKS", 4.5, error));
	CHECK(pipeline->SetSetting("PeakExtractor", "MAX_PEAKS", 4, error));
	CHECK(pipeline->SetSetting("PeakExtractor", "MIN_SNR", 1.5, error));
}

TEST_CASE("A changed setting reaches the stage on the next frame")
{
	const Adagio::AudioFrame frame = MakeToneFrame(440.0f, 8000, 4096);
	std::unique_ptr<Adagio::AnalysisPipeline> pipeline = MakePipeline();
	std::string error;

	const size_t defaultPeaks = pipeline->ProcessFrame(frame)->Context->Peaks.size();
	REQUIRE(defaultPeaks > 1);

	REQUIRE(pipeline->SetSetting("PeakExtractor", "MAX_PEAKS", 1, error));
	CHECK(pipeline->ProcessFrame(frame)->Context->Peaks.size() == 1);

	// And the schema reports what it is running with, not what it shipped with.
	const nlohmann::json maxPeaks = FindSetting(pipeline->GetSchemaJson(), "PeakExtractor", "MAX_PEAKS");
	REQUIRE_FALSE(maxPeaks.is_null());
	CHECK(maxPeaks.at("value") == 1);
	CHECK(maxPeaks.at("default") != 1);
}

TEST_CASE("The A4 reference is range-checked and names notes against itself")
{
	std::unique_ptr<Adagio::AnalysisPipeline> pipeline = MakePipeline();
	std::string error;

	// The schema reports the default before anything is set: the field is in the
	// settings macro under the schema's own key.
	const nlohmann::json tuning = FindSetting(pipeline->GetSchemaJson(), "NoteDetector", "A440_TUNING");
	REQUIRE_FALSE(tuning.is_null());
	CHECK(tuning.at("value") == doctest::Approx(440.0));

	CHECK_FALSE(pipeline->SetSetting("NoteDetector", "A440_TUNING", 400.0, error));
	CHECK(error.find("A440_TUNING") != std::string::npos);
	CHECK(pipeline->SetSetting("NoteDetector", "A440_TUNING", 442.0, error));

	// 452.9 Hz is about 50 cents sharp of A4 at 440, past the error threshold, so it
	// names nothing. Named against 452.9, it is A4 to within a few cents.
	// Each result is held, not iterated straight off ProcessFrame: the loop would bind
	// to a member of a temporary that is gone before the body runs.
	const Adagio::AudioFrame frame = MakeToneFrame(452.9f, 8000, 4096);
	REQUIRE(pipeline->SetSetting("NoteDetector", "A440_TUNING", 440.0, error));
	const auto atDefault = pipeline->ProcessFrame(frame);
	for (const auto& note : atDefault->Context->Notes)
		CHECK(note.Midi != 69);

	REQUIRE(pipeline->SetSetting("NoteDetector", "A440_TUNING", 452.9, error));
	const auto retuned = pipeline->ProcessFrame(frame);
	bool foundA4 = false;
	for (const auto& note : retuned->Context->Notes)
	{
		if (note.Midi != 69)
			continue;
		foundA4 = true;
		CHECK(std::abs(note.ErrorCents) < 5.0f);
	}
	CHECK(foundA4);
}

// A stage's name is the key its settings are stored and sent under. It used to be
// derived from typeid, which only reads this way on MSVC; now each stage spells it,
// and a rename that changed a key would break every client's stored settings.
TEST_CASE("the production stages keep their settings keys, in pipeline order")
{
	const nlohmann::json schema = Adagio::AnalysisService::CreatePipeline()->GetSchemaJson();
	std::vector<std::string> names;
	for (const auto& stage : schema.at("stages"))
		names.push_back(stage.at("name").get<std::string>());

	const std::vector<std::string> expected = {
		"FFTProcessor", "HPSDownsamplerProcessor", "SpectrumFilterProcessor",
		"PeakExtractor", "NoteDetector", "KeyDetector", "ChordPredictor"
	};
	CHECK(names == expected);
}
