// RingBuffer behaviour that the analysis read path depends on.
//
// AnalysisService keeps the whole preprocessed stream in a RingBuffer and reads
// frames out of it with the readFrom override, treating it as random-access
// storage rather than a stream. A frame near the end of the track therefore wraps
// around to the start of the song (A7). Phase 2 fixes this by holding the stream
// in a std::vector and copying a zero-padded slice; when it does, retarget the
// should_fail case below at that code path.
#include "../src/Buffers/RingBuffer.h"

#include <doctest/doctest.h>

#include <vector>

TEST_CASE("RingBuffer round-trips what was written")
{
	Adagio::RingBuffer<float> buffer(64);
	std::vector<float> source(40);
	for (size_t i = 0; i < source.size(); ++i)
		source[i] = static_cast<float>(i) + 1.0f;

	CHECK(buffer.Write(source.data(), source.size()) == source.size());
	CHECK(buffer.GetAvailableCount() == source.size());

	std::vector<float> out(source.size(), -1.0f);
	CHECK(buffer.Read(out.data(), out.size()) == source.size());
	CHECK(out == source);
	CHECK(buffer.GetAvailableCount() == 0);
}

TEST_CASE("RingBuffer writes stop at the free capacity")
{
	Adagio::RingBuffer<float> buffer(8);
	std::vector<float> source(16, 1.0f);

	// One slot is reserved to tell "full" from "empty".
	CHECK(buffer.Write(source.data(), source.size()) == 7);
	CHECK(buffer.Write(source.data(), source.size()) == 0);
}

TEST_CASE("A7: a read past the end of the stream is not filled from the start" * doctest::should_fail())
{
	Adagio::RingBuffer<float> buffer(1024);
	std::vector<float> source(1000);
	for (size_t i = 0; i < source.size(); ++i)
		source[i] = static_cast<float>(i) + 1.0f;
	REQUIRE(buffer.Write(source.data(), source.size()) == source.size());

	// A frame centred near the end of the track: 20 real samples remain, and the
	// rest of the request lies past everything that was written.
	std::vector<float> out(100, -1.0f);
	buffer.Read(out.data(), out.size(), 980);

	CHECK(out[0] == doctest::Approx(source[980]));
	CHECK(out[19] == doctest::Approx(source[999]));

	// Index 44 is where the physical buffer wraps, and it currently comes back
	// holding source[0] — the opening samples of the song.
	CHECK(out[44] == doctest::Approx(0.0f));
	CHECK(out[99] == doctest::Approx(0.0f));
}

TEST_CASE("T6: Clear leaves the buffer usable")
{
	// Clear() calls m_Buffer.clear(), which sets size() to 0 while leaving the
	// capacity behind, so every later memcpy into data() is out of bounds.
	Adagio::RingBuffer<float> buffer(64);
	std::vector<float> source(40);
	for (size_t i = 0; i < source.size(); ++i)
		source[i] = static_cast<float>(i) + 1.0f;

	REQUIRE(buffer.Write(source.data(), source.size()) == source.size());
	buffer.Clear();
	CHECK(buffer.GetAvailableCount() == 0);

	CHECK(buffer.Write(source.data(), source.size()) == source.size());
	std::vector<float> out(source.size(), -1.0f);
	CHECK(buffer.Read(out.data(), out.size()) == source.size());
	CHECK(out == source);
}

// T10: dropping the whole buffer on a seek also dropped post-seek audio the feeder had already written.
TEST_CASE("T10: DropToMark drops what came before the mark and keeps what came after")
{
	Adagio::RingBuffer<float> buffer(16);

	// Park both indices near the end of storage so the kept audio wraps around.
	std::vector<float> filler(12, 0.0f);
	REQUIRE(buffer.Write(filler.data(), filler.size()) == filler.size());
	REQUIRE(buffer.Read(filler.data(), filler.size()) == filler.size());

	std::vector<float> beforeSeek(6, -1.0f);
	std::vector<float> afterSeek = { 1.0f, 2.0f, 3.0f, 4.0f, 5.0f };
	REQUIRE(buffer.Write(beforeSeek.data(), beforeSeek.size()) == beforeSeek.size());
	buffer.MarkWritePosition(7);
	REQUIRE(buffer.Write(afterSeek.data(), afterSeek.size()) == afterSeek.size());

	CHECK(buffer.DropToMark(7));
	CHECK(buffer.GetAvailableCount() == afterSeek.size());

	std::vector<float> out(afterSeek.size(), 0.0f);
	CHECK(buffer.Read(out.data(), out.size()) == afterSeek.size());
	CHECK(out == afterSeek);
}

TEST_CASE("T10: DropToMark waits for the mark carrying its own tag")
{
	Adagio::RingBuffer<float> buffer(16);
	std::vector<float> source(4, 1.0f);
	REQUIRE(buffer.Write(source.data(), source.size()) == source.size());

	CHECK_FALSE(buffer.DropToMark(1));
	buffer.MarkWritePosition(1);
	CHECK_FALSE(buffer.DropToMark(2));
	CHECK(buffer.GetAvailableCount() == source.size());
}

TEST_CASE("T10: DropToMark never moves the reader backwards")
{
	// The callback can read past a fresh mark in the block before it notices the seek.
	Adagio::RingBuffer<float> buffer(32);
	std::vector<float> source(10, 1.0f);
	REQUIRE(buffer.Write(source.data(), source.size()) == source.size());
	buffer.MarkWritePosition(3);
	REQUIRE(buffer.Write(source.data(), source.size()) == source.size());

	std::vector<float> out(15);
	REQUIRE(buffer.Read(out.data(), out.size()) == out.size());

	CHECK(buffer.DropToMark(3));
	CHECK(buffer.GetAvailableCount() == 5);
}
