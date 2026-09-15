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
