// Loading pipelines.json: the built-ins take only their stored values, custom pipelines load as written, and a file
// it can't use is set aside whole. Saving writes what a load reads back, 500 ms after the last change.
#include "../src/Analysis/PipelineStore.h"
#include "../src/Core/MessageQueue.h"

#include <doctest/doctest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace
{
	using Adagio::PipelineDefinition;
	using Adagio::PipelineStore;

	class TempFolder
	{
	public:
		TempFolder()
			: m_Path(std::filesystem::temp_directory_path() / ("AdagioPipelineStore-" + std::to_string(std::random_device{}())))
		{
			std::filesystem::create_directories(m_Path);
		}

		~TempFolder()
		{
			std::error_code error;
			std::filesystem::remove_all(m_Path, error);
		}

		std::filesystem::path File(const std::string& name) const { return m_Path / name; }

	private:
		std::filesystem::path m_Path;
	};

	void WriteText(const std::filesystem::path& path, const std::string& text)
	{
		std::ofstream file(path, std::ios::binary);
		file << text;
	}

	std::string ReadText(const std::filesystem::path& path)
	{
		std::ifstream file(path, std::ios::binary);
		std::stringstream text;
		text << file.rdbuf();
		return text.str();
	}

	std::vector<std::string> StageNames(const PipelineDefinition& pipeline)
	{
		std::vector<std::string> names;
		for (const auto& stage : pipeline.Stages)
			names.push_back(stage.StageID);
		return names;
	}

	const nlohmann::json* FindSettings(const PipelineDefinition& pipeline, const std::string& stage)
	{
		for (const auto& entry : pipeline.Stages)
		{
			if (entry.StageID == stage)
				return &entry.Settings;
		}
		return nullptr;
	}

	PipelineStore LoadDefaults()
	{
		PipelineStore defaults(std::filesystem::path{});
		defaults.Load();
		return defaults;
	}
}

TEST_CASE("With no file, or no path, the built-ins load with no values and no warning")
{
	TempFolder folder;
	const std::filesystem::path file = folder.File("pipelines.json");

	for (const std::filesystem::path& path : { file, std::filesystem::path{} })
	{
		PipelineStore store(path);
		CHECK(store.Load().empty());

		const PipelineDefinition* active = store.Find(store.GetActiveId());
		REQUIRE(active != nullptr);
		CHECK(active->BuiltIn);

		const PipelineDefinition* hps = store.Find("hps");
		REQUIRE(hps != nullptr);
		for (const auto& stage : hps->Stages)
			CHECK(stage.Settings == nlohmann::json::object());
	}
	CHECK_FALSE(std::filesystem::exists(file));
	CHECK_FALSE(std::filesystem::exists(folder.File("pipelines.json.bad")));
}

TEST_CASE("A built-in takes the values of the stages it still has, and keeps the code's stage list and name")
{
	TempFolder folder;
	const std::filesystem::path file = folder.File("pipelines.json");
	WriteText(file, R"({
		"version": 1,
		"active": "hps",
		"pipelines": [
			{ "id": "hps", "name": "Renamed", "builtIn": true, "stages": [
				{ "stage": "FFTProcessor", "settings": { "WINDOW": "Hann" } },
				{ "stage": "RemovedStage", "settings": { "X": 1 } },
				{ "stage": "KeyDetector" }
			] }
		]
	})");

	PipelineStore store(file);
	CHECK(store.Load().empty());
	CHECK(store.GetActiveId() == "hps");

	const PipelineStore defaults = LoadDefaults();
	const PipelineDefinition* hps = store.Find("hps");
	REQUIRE(hps != nullptr);
	CHECK(hps->BuiltIn);
	CHECK(hps->Name == defaults.Find("hps")->Name);
	CHECK(StageNames(*hps) == StageNames(*defaults.Find("hps")));

	REQUIRE(FindSettings(*hps, "FFTProcessor") != nullptr);
	CHECK(*FindSettings(*hps, "FFTProcessor") == nlohmann::json{ {"WINDOW", "Hann"} });
	CHECK(*FindSettings(*hps, "KeyDetector") == nlohmann::json::object());
	CHECK(FindSettings(*hps, "RemovedStage") == nullptr);
}

TEST_CASE("A custom pipeline loads as written and may be the active one")
{
	TempFolder folder;
	const std::filesystem::path file = folder.File("pipelines.json");
	WriteText(file, R"({
		"active": "mine",
		"pipelines": [
			{ "id": "mine", "name": "Mine", "description": "Peaks only", "stages": [
				{ "stage": "FFTProcessor", "settings": { "WINDOW": "Hann" } },
				{ "stage": "PeakExtractor" }
			] }
		]
	})");

	PipelineStore store(file);
	CHECK(store.Load().empty());
	CHECK(store.GetActiveId() == "mine");

	const PipelineDefinition* mine = store.Find("mine");
	REQUIRE(mine != nullptr);
	CHECK_FALSE(mine->BuiltIn);
	CHECK(mine->Name == "Mine");
	CHECK(mine->Description == "Peaks only");
	CHECK(StageNames(*mine) == std::vector<std::string>{ "FFTProcessor", "PeakExtractor" });
	CHECK(*FindSettings(*mine, "FFTProcessor") == nlohmann::json{ {"WINDOW", "Hann"} });
	CHECK(store.Find("hps") != nullptr);
}

TEST_CASE("An active id that names no pipeline falls back to the default without a warning")
{
	TempFolder folder;
	const std::filesystem::path file = folder.File("pipelines.json");
	WriteText(file, R"({ "active": "gone", "pipelines": [] })");

	PipelineStore store(file);
	CHECK(store.Load().empty());
	CHECK(store.GetActiveId() == LoadDefaults().GetActiveId());
	CHECK_FALSE(std::filesystem::exists(folder.File("pipelines.json.bad")));
}

TEST_CASE("A file it can't use is copied aside whole, and the built-ins load with one warning")
{
	std::string text;
	SUBCASE("not JSON") { text = "{ \"active\": "; }
	SUBCASE("empty") { text = ""; }
	SUBCASE("not the file's shape") { text = R"([ "hps" ])"; }
	SUBCASE("no active") { text = R"({ "pipelines": [] })"; }
	SUBCASE("a pipeline with no id") { text = R"({ "active": "hps", "pipelines": [ { "name": "x", "stages": [] } ] })"; }
	SUBCASE("an id used twice") { text = R"({ "active": "hps", "pipelines": [ { "id": "hps", "stages": [] }, { "id": "hps", "stages": [] } ] })"; }
	SUBCASE("a custom pipeline with no name") { text = R"({ "active": "hps", "pipelines": [ { "id": "mine", "stages": [ { "stage": "FFTProcessor" } ] } ] })"; }
	SUBCASE("a custom pipeline with no stages") { text = R"({ "active": "hps", "pipelines": [ { "id": "mine", "name": "Mine", "stages": [] } ] })"; }
	SUBCASE("an unknown stage") { text = R"({ "active": "hps", "pipelines": [ { "id": "mine", "name": "Mine", "stages": [ { "stage": "Nope" } ] } ] })"; }
	SUBCASE("a stage listed twice") { text = R"({ "active": "hps", "pipelines": [ { "id": "mine", "name": "Mine", "stages": [ { "stage": "FFTProcessor" }, { "stage": "FFTProcessor" } ] } ] })"; }
	SUBCASE("settings that aren't an object") { text = R"({ "active": "hps", "pipelines": [ { "id": "hps", "stages": [ { "stage": "FFTProcessor", "settings": [] } ] } ] })"; }

	TempFolder folder;
	const std::filesystem::path file = folder.File("pipelines.json");
	WriteText(file, text);

	PipelineStore store(file);
	const std::vector<std::string> warnings = store.Load();
	REQUIRE(warnings.size() == 1);
	CHECK(warnings[0].rfind("Ignoring pipelines.json: ", 0) == 0);
	CHECK(warnings[0].find("pipelines.json.bad") != std::string::npos);

	CHECK(ReadText(folder.File("pipelines.json.bad")) == text);
	CHECK(store.GetActiveId() == LoadDefaults().GetActiveId());
	CHECK(store.Find("mine") == nullptr);
	for (const auto& stage : store.Find("hps")->Stages)
		CHECK(stage.Settings == nlohmann::json::object());
}

TEST_CASE("A bad custom pipeline sets aside the built-in values stored beside it too")
{
	TempFolder folder;
	const std::filesystem::path file = folder.File("pipelines.json");
	WriteText(file, R"({
		"active": "hps",
		"pipelines": [
			{ "id": "hps", "stages": [ { "stage": "FFTProcessor", "settings": { "WINDOW": "Hann" } } ] },
			{ "id": "mine", "name": "Mine", "stages": [ { "stage": "Nope" } ] }
		]
	})");

	PipelineStore store(file);
	const std::vector<std::string> warnings = store.Load();
	REQUIRE(warnings.size() == 1);
	CHECK(warnings[0] == "Ignoring pipelines.json: pipeline 'mine' has a stage the engine doesn't know: Nope. Kept a copy as pipelines.json.bad.");
	CHECK(*FindSettings(*store.Find("hps"), "FFTProcessor") == nlohmann::json::object());
	CHECK(store.GetActiveId() == LoadDefaults().GetActiveId());
}

TEST_CASE("Save writes what a second store loads back, replacing the file it read")
{
	TempFolder folder;
	const std::filesystem::path file = folder.File("pipelines.json");
	WriteText(file, R"({
		"active": "hps",
		"pipelines": [
			{ "id": "mine", "name": "Mine", "description": "Peaks only", "stages": [
				{ "stage": "FFTProcessor", "settings": { "WINDOW": "Hann" } },
				{ "stage": "PeakExtractor" }
			] }
		]
	})");

	PipelineStore store(file);
	REQUIRE(store.Load().empty());
	store.SetValues("hps", { {"KeyDetector", { {"ROLLING_WINDOW", 6.0} }} });
	store.SetActiveId("mine");
	store.Save();
	CHECK_FALSE(std::filesystem::exists(folder.File("pipelines.json.tmp")));

	PipelineStore reloaded(file);
	CHECK(reloaded.Load().empty());
	CHECK(reloaded.GetActiveId() == "mine");

	const PipelineDefinition* hps = reloaded.Find("hps");
	REQUIRE(hps != nullptr);
	CHECK(StageNames(*hps) == StageNames(*store.Find("hps")));
	CHECK(*FindSettings(*hps, "KeyDetector") == nlohmann::json{ {"ROLLING_WINDOW", 6.0} });
	CHECK(*FindSettings(*hps, "FFTProcessor") == nlohmann::json::object());

	const PipelineDefinition* mine = reloaded.Find("mine");
	REQUIRE(mine != nullptr);
	CHECK_FALSE(mine->BuiltIn);
	CHECK(mine->Name == "Mine");
	CHECK(mine->Description == "Peaks only");
	CHECK(StageNames(*mine) == std::vector<std::string>{ "FFTProcessor", "PeakExtractor" });
	CHECK(*FindSettings(*mine, "FFTProcessor") == nlohmann::json{ {"WINDOW", "Hann"} });
	CHECK(*FindSettings(*mine, "PeakExtractor") == nlohmann::json::object());
}

TEST_CASE("SaveIfDue writes once, 500 ms after the last change")
{
	using namespace std::chrono_literals;

	TempFolder folder;
	const std::filesystem::path file = folder.File("pipelines.json");
	const std::chrono::steady_clock::time_point start{};

	PipelineStore store(file);
	store.Load();
	store.SaveIfDue(start);
	store.SaveIfDue(start + 499ms);
	CHECK_FALSE(std::filesystem::exists(file));

	store.SetActiveId("hps");
	store.SaveIfDue(start + 499ms);
	store.SaveIfDue(start + 998ms);
	CHECK_FALSE(std::filesystem::exists(file));

	store.SaveIfDue(start + 999ms);
	REQUIRE(std::filesystem::exists(file));

	PipelineStore reloaded(file);
	CHECK(reloaded.Load().empty());
	CHECK(reloaded.GetActiveId() == "hps");

	std::filesystem::remove(file);
	store.SaveIfDue(start + 2000ms);
	store.Save();
	CHECK_FALSE(std::filesystem::exists(file));
}

TEST_CASE("A failed write is one error event, and isn't retried until the next change")
{
	using namespace std::chrono_literals;

	TempFolder folder;
	const std::filesystem::path file = folder.File("pipelines.json");
	std::filesystem::create_directory(file);

	Adagio::OutboundMessage message;
	while (Adagio::MessageQueue::GetInstance().Pop(message)) {}

	PipelineStore store(file);
	store.Load();
	store.Save();
	CHECK_FALSE(std::filesystem::exists(folder.File("pipelines.json.tmp")));

	std::vector<nlohmann::json> errors;
	while (Adagio::MessageQueue::GetInstance().Pop(message))
		errors.push_back(nlohmann::json::parse(message.Payload));
	REQUIRE(errors.size() == 1);
	CHECK(errors[0]["type"] == "error");
	CHECK(errors[0]["value"].get<std::string>().rfind("Couldn't save the analysis settings: ", 0) == 0);

	const std::chrono::steady_clock::time_point start{};
	store.SaveIfDue(start);
	store.SaveIfDue(start + 1000ms);
	store.Save();
	CHECK_FALSE(Adagio::MessageQueue::GetInstance().Pop(message));
}
