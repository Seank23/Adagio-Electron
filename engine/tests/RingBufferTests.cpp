// RingBuffer behaviour that the analysis read path depends on.
//
// AnalysisService keeps the whole preprocessed stream in a RingBuffer and reads
// frames out of it with the readFrom override, treating it as random-access
// storage rather than a stream (A7). The stream stays there for now, so these
// cases pin what that override guarantees: a read near the end comes back short
// instead of wrapping, and it leaves the streaming reader alone. One hole is
// still open, and carries a should_fail case below.
//
// Note that Write reserves a slot to tell "full" from "empty", so a buffer sized
// to the stream holds all of it but the last sample.
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

namespace
{
	// A stream of 1,000 samples in a buffer with room to spare, as AnalysisService
	// holds the preprocessed track.
	void FillStream(Adagio::RingBuffer<float>& buffer, std::vector<float>& outSource)
	{
		outSource.resize(1000);
		for (size_t i = 0; i < outSource.size(); ++i)
			outSource[i] = static_cast<float>(i) + 1.0f;
		REQUIRE(buffer.Write(outSource.data(), outSource.size()) == outSource.size());
	}
}

TEST_CASE("A7: a read past the end of the stream is not filled from the start")
{
	std::vector<float> source;
	Adagio::RingBuffer<float> buffer(1024);
	FillStream(buffer, source);

	// A frame centred near the end of the track: 20 real samples remain, and the
	// rest of the request lies past everything that was written. The available
	// count is measured from readFrom, so the read comes back short rather than
	// wrapping round to the opening of the song.
	std::vector<float> out(100, -1.0f);
	CHECK(buffer.Read(out.data(), out.size(), 980) == 20);

	CHECK(out[0] == doctest::Approx(source[980]));
	CHECK(out[19] == doctest::Approx(source[999]));

	// Index 44 is where the physical buffer wraps. Nothing beyond the short read is
	// written, so the caller's own initial value survives - which is how the frame
	// ends up zero-padded: AnalysisService reads into a fresh univector.
	CHECK(out[44] == doctest::Approx(-1.0f));
	CHECK(out[99] == doctest::Approx(-1.0f));

	std::vector<float> zeroed(100, 0.0f);
	CHECK(buffer.Read(zeroed.data(), zeroed.size(), 980) == 20);
	CHECK(zeroed[44] == doctest::Approx(0.0f));
	CHECK(zeroed[99] == doctest::Approx(0.0f));
}

TEST_CASE("A7: reading a frame does not move the streaming reader")
{
	// The same buffer is a stream to its reader and random-access storage to the
	// analysis thread. A frame read must not consume anything.
	std::vector<float> source;
	Adagio::RingBuffer<float> buffer(1024);
	FillStream(buffer, source);

	std::vector<float> frame(64, 0.0f);
	CHECK(buffer.Read(frame.data(), frame.size(), 500) == frame.size());
	CHECK(frame[0] == doctest::Approx(source[500]));
	CHECK(buffer.GetAvailableCount() == source.size());

	std::vector<float> streamed(10, 0.0f);
	CHECK(buffer.Read(streamed.data(), streamed.size()) == streamed.size());
	CHECK(streamed[0] == doctest::Approx(source[0]));
	CHECK(buffer.GetAvailableCount() == source.size() - streamed.size());
}

TEST_CASE("A7: a frame starting past the last written sample is empty" * doctest::should_fail())
{
	// What is left of A7 while the stream lives in a ring buffer. AnalysisService
	// clamps the frame start to GetCapacity(), and at the very end of a track the
	// estimate reaches it: the modulo then makes the whole buffer look available
	// and the read is served from index 0 - the opening of the song, played back as
	// the analysis of its final moment. Fix it by clamping the caller to the last
	// written sample, or by refusing an out-of-range readFrom here.
	std::vector<float> source;
	Adagio::RingBuffer<float> buffer(1024);
	FillStream(buffer, source);

	std::vector<float> out(100, -1.0f);
	CHECK(buffer.Read(out.data(), out.size(), buffer.GetCapacity()) == 0);
	CHECK(out[0] == doctest::Approx(-1.0f));
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
