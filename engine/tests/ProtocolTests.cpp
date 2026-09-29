// The wire protocol, checked against protocol.json's own vocabulary.
//
// Everything here is pure: parsing a frame never touches the transport, so a bad
// frame can be proven to stop at the parser rather than reaching the command queue.
#include "../src/Core/BinaryFrame.h"
#include "../src/Core/CommandParser.h"

#include <doctest/doctest.h>

#include <cstring>
#include <string>
#include <vector>

namespace
{
	using Adagio::CommandType;

	// Reads a field the way the renderer does: at the offset protocol.json gives it.
	template <typename T>
	T ReadAt(const std::string& frame, size_t offset)
	{
		T value{};
		std::memcpy(&value, frame.data() + offset, sizeof(T));
		return value;
	}

	const std::vector<CommandType> AllCommands = {
#define ADAGIO_TEST_COMMAND(cppName, wireName, argKind) CommandType::cppName,
		ADAGIO_PROTOCOL_COMMANDS(ADAGIO_TEST_COMMAND)
#undef ADAGIO_TEST_COMMAND
	};
}

TEST_CASE("Every command name round-trips through the generated table")
{
	for (CommandType command : AllCommands)
	{
		const std::string name = Adagio::ToString(command);
		CAPTURE(name);
		CHECK(name != "unknown");

		CommandType parsed = CommandType::Shutdown;
		REQUIRE(Adagio::CommandFromString(name, parsed));
		CHECK(parsed == command);
	}
}

TEST_CASE("A command with no argument parses and carries its id and client")
{
	const Adagio::ParsedCommand parsed = Adagio::ParseCommand(R"({"id":"7","cmd":"play"})", "client-1");

	REQUIRE(parsed.Ok);
	CHECK(parsed.Value.Type == CommandType::Play);
	CHECK(parsed.RequestId == "7");
	CHECK(parsed.Value.RequestId == "7");
	CHECK(parsed.Value.ClientId == "client-1");
}

TEST_CASE("An id may be a number, and is echoed rather than interpreted")
{
	CHECK(Adagio::ParseCommand(R"({"id":12,"cmd":"play"})", "c").RequestId == "12");
	// No id means nobody is waiting: the command still runs, it just goes unanswered.
	CHECK(Adagio::ParseCommand(R"({"cmd":"play"})", "c").RequestId.empty());
}

TEST_CASE("Malformed frames are refused rather than queued")
{
	CHECK_FALSE(Adagio::ParseCommand("not json", "c").Ok);
	CHECK_FALSE(Adagio::ParseCommand("[1,2,3]", "c").Ok);
	CHECK_FALSE(Adagio::ParseCommand(R"({"args":3})", "c").Ok);
	CHECK_FALSE(Adagio::ParseCommand(R"({"cmd":42})", "c").Ok);

	const Adagio::ParsedCommand unknown = Adagio::ParseCommand(R"({"id":"9","cmd":"selfDestruct"})", "c");
	CHECK_FALSE(unknown.Ok);
	// The refusal still has to be addressable, or the caller waits forever.
	CHECK(unknown.RequestId == "9");
	CHECK(unknown.Error.find("selfDestruct") != std::string::npos);
}

TEST_CASE("Arguments are checked against the kind the protocol declares")
{
	SUBCASE("number")
	{
		const Adagio::ParsedCommand parsed = Adagio::ParseCommand(R"({"cmd":"seek","args":12.5})", "c");
		REQUIRE(parsed.Ok);
		CHECK(parsed.Value.Value == doctest::Approx(12.5f));

		CHECK_FALSE(Adagio::ParseCommand(R"({"cmd":"seek","args":"12.5"})", "c").Ok);
		CHECK_FALSE(Adagio::ParseCommand(R"({"cmd":"seek"})", "c").Ok);
	}

	SUBCASE("string")
	{
		const Adagio::ParsedCommand parsed = Adagio::ParseCommand(R"({"cmd":"load","args":"C:\\music\\a.wav"})", "c");
		REQUIRE(parsed.Ok);
		CHECK(parsed.Value.StrValue == "C:\\music\\a.wav");

		CHECK_FALSE(Adagio::ParseCommand(R"({"cmd":"load","args":3})", "c").Ok);
	}

	SUBCASE("object")
	{
		const Adagio::ParsedCommand parsed = Adagio::ParseCommand(R"({"cmd":"setAnalysisSetting","args":{"stage":"PeakExtractor","key":"MAX_PEAKS","value":12}})", "c");
		REQUIRE(parsed.Ok);
		CHECK(parsed.Value.Args.at("stage") == "PeakExtractor");
		CHECK(parsed.Value.Args.at("value") == 12);

		CHECK_FALSE(Adagio::ParseCommand(R"({"cmd":"setAnalysisSetting","args":"MAX_PEAKS"})", "c").Ok);
	}

	SUBCASE("none ignores whatever it is given")
	{
		CHECK(Adagio::ParseCommand(R"({"cmd":"play","args":"ignored"})", "c").Ok);
	}
}

TEST_CASE("A reply carries a value or an error, never both")
{
	const nlohmann::json ok = nlohmann::json::parse(Adagio::ReplyJson("3", true, { {"state", "ready"} }, ""));
	CHECK(ok.at("type") == "reply");
	CHECK(ok.at("id") == "3");
	CHECK(ok.at("ok") == true);
	CHECK(ok.at("value").at("state") == "ready");
	CHECK_FALSE(ok.contains("error"));

	const nlohmann::json failed = nlohmann::json::parse(Adagio::ReplyJson("4", false, {}, "No audio file is loaded."));
	CHECK(failed.at("ok") == false);
	CHECK(failed.at("error") == "No audio file is loaded.");
	CHECK_FALSE(failed.contains("value"));
}

TEST_CASE("A spectrum frame carries its header at the declared offsets and its data after it")
{
	namespace Binary = Adagio::Protocol::Binary;
	const std::vector<float> magnitudes = { 0.0f, 2.0f, 8.0f, 4.0f };
	const std::string frame = Adagio::BinaryFrame::EncodeSpectrum(magnitudes, 1.953125f, 12.5, 7);

	REQUIRE(frame.size() == Binary::HeaderSize + magnitudes.size() * sizeof(uint16_t));
	CHECK(ReadAt<uint8_t>(frame, Binary::Offset::Kind) == (uint8_t)Binary::FrameKind::Spectrum);
	CHECK(ReadAt<uint8_t>(frame, Binary::Offset::Version) == Adagio::Protocol::Version);
	CHECK(ReadAt<uint16_t>(frame, Binary::Offset::ElementType) == (uint16_t)Binary::ElementType::Uint16);
	CHECK(ReadAt<uint32_t>(frame, Binary::Offset::SeekGeneration) == 7);
	CHECK(ReadAt<double>(frame, Binary::Offset::Timestamp) == 12.5);
	CHECK(ReadAt<float>(frame, Binary::Offset::Resolution) == 1.953125f);
	CHECK(ReadAt<float>(frame, Binary::Offset::MaxMagnitude) == 8.0f);
	CHECK(ReadAt<uint32_t>(frame, Binary::Offset::Count) == magnitudes.size());
	CHECK(ReadAt<uint32_t>(frame, Binary::Offset::Reserved) == 0);

	// Each element is its magnitude as a fraction of the maximum, so the maximum is full scale.
	std::vector<uint16_t> data(magnitudes.size());
	std::memcpy(data.data(), frame.data() + Binary::HeaderSize, data.size() * sizeof(uint16_t));
	CHECK(data[0] == 0);
	CHECK(data[2] == 65535);
	for (size_t i = 0; i < magnitudes.size(); ++i)
	{
		CAPTURE(i);
		const float decoded = data[i] / 65535.0f * 8.0f;
		CHECK(decoded == doctest::Approx(magnitudes[i]).epsilon(1e-4));
	}
}

TEST_CASE("A silent spectrum encodes as zeros rather than dividing by a zero maximum")
{
	const std::vector<float> silence(16, 0.0f);
	const std::string frame = Adagio::BinaryFrame::EncodeSpectrum(silence, 1.0f, 0.0, 0);

	CHECK(ReadAt<float>(frame, Adagio::Protocol::Binary::Offset::MaxMagnitude) == 0.0f);
	for (size_t i = 0; i < silence.size(); ++i)
		CHECK(ReadAt<uint16_t>(frame, Adagio::Protocol::Binary::HeaderSize + i * sizeof(uint16_t)) == 0);
}

TEST_CASE("A waveform frame keeps its peaks as Float32 and names its resolution")
{
	namespace Binary = Adagio::Protocol::Binary;
	const std::vector<float> peaks = { 0.25f, -0.5f, 0.75f };
	const std::string frame = Adagio::BinaryFrame::EncodeWaveform(peaks, 2048);

	REQUIRE(frame.size() == Binary::HeaderSize + peaks.size() * sizeof(float));
	CHECK(ReadAt<uint8_t>(frame, Binary::Offset::Kind) == (uint8_t)Binary::FrameKind::Waveform);
	CHECK(ReadAt<uint16_t>(frame, Binary::Offset::ElementType) == (uint16_t)Binary::ElementType::Float32);
	CHECK(ReadAt<float>(frame, Binary::Offset::Resolution) == 2048.0f);
	CHECK(ReadAt<uint32_t>(frame, Binary::Offset::Count) == peaks.size());
	for (size_t i = 0; i < peaks.size(); ++i)
		CHECK(ReadAt<float>(frame, Binary::HeaderSize + i * sizeof(float)) == peaks[i]);
}
