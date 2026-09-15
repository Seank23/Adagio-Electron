// T11: RubberBand releases its start delay only after a final process() call.
#include "../src/Core/TimeProcessor.h"
#include "../src/Buffers/RingBuffer.h"

#include <doctest/doctest.h>

#include <cmath>
#include <vector>

namespace
{
	constexpr int SAMPLE_RATE = 44100;
	constexpr int CHANNELS = 2;
	constexpr size_t BLOCK_FRAMES = 512;
	// Only stops a regression from spinning forever.
	constexpr int MAX_BLOCKS = 1000;

	void WriteTone(Adagio::RingBuffer<float>& buffer, size_t frames)
	{
		std::vector<float> samples(frames * CHANNELS);
		for (size_t i = 0; i < frames; ++i)
		{
			const float value = 0.5f * static_cast<float>(std::sin(2.0 * 3.141592653589793 * 440.0 * static_cast<double>(i) / SAMPLE_RATE));
			for (int ch = 0; ch < CHANNELS; ++ch)
				samples[i * CHANNELS + ch] = value;
		}
		REQUIRE(buffer.Write(samples.data(), samples.size()) == samples.size());
	}

	size_t CapacityFor(double seconds)
	{
		return static_cast<size_t>(seconds * SAMPLE_RATE * CHANNELS);
	}
}

TEST_CASE("T11: a stretched track drains once its source is exhausted")
{
	Adagio::RingBuffer<float> buffer(CapacityFor(5.0));
	Adagio::TimeProcessor processor;
	processor.Init(SAMPLE_RATE, CHANNELS, &buffer, BLOCK_FRAMES);
	processor.SetSpeed(0.5f);

	WriteTone(buffer, SAMPLE_RATE / 2);

	std::vector<float> output(BLOCK_FRAMES * CHANNELS);
	bool drained = false;
	int blocks = 0;
	while (!drained && blocks < MAX_BLOCKS)
	{
		drained = processor.ProcessAudio(output.data(), output.size(), true).Drained;
		++blocks;
	}

	CHECK(drained);
	CHECK(buffer.GetAvailableCount() == 0);
	// 43 blocks unstretched, about 86 at half speed; fewer than 60 means the stretch was cut short.
	CHECK(blocks > 60);

	const Adagio::ProcessAudioResult after = processor.ProcessAudio(output.data(), output.size(), true);
	CHECK(after.Drained);
	CHECK(after.FramesConsumed == 0);
	bool silent = true;
	for (float sample : output)
		silent = silent && sample == 0.0f;
	CHECK(silent);
}

TEST_CASE("T11: an empty buffer is not the end while the feeder may still write")
{
	Adagio::RingBuffer<float> buffer(CapacityFor(5.0));
	Adagio::TimeProcessor processor;
	processor.Init(SAMPLE_RATE, CHANNELS, &buffer, BLOCK_FRAMES);
	processor.SetSpeed(0.5f);

	WriteTone(buffer, SAMPLE_RATE / 10);

	std::vector<float> output(BLOCK_FRAMES * CHANNELS);
	for (int blocks = 0; blocks < 40; ++blocks)
		CHECK_FALSE(processor.ProcessAudio(output.data(), output.size(), false).Drained);
	REQUIRE(buffer.GetAvailableCount() == 0);

	// A stretcher already sent its final block would refuse this and consume nothing.
	WriteTone(buffer, SAMPLE_RATE / 10);
	size_t framesConsumed = 0;
	for (int blocks = 0; blocks < 10; ++blocks)
		framesConsumed += processor.ProcessAudio(output.data(), output.size(), false).FramesConsumed;
	CHECK(framesConsumed > 0);
}

TEST_CASE("T11: at 100% speed the end is the first short read of an exhausted source")
{
	Adagio::RingBuffer<float> buffer(CapacityFor(1.0));
	Adagio::TimeProcessor processor;
	processor.Init(SAMPLE_RATE, CHANNELS, &buffer, BLOCK_FRAMES);

	WriteTone(buffer, BLOCK_FRAMES + BLOCK_FRAMES / 2);

	std::vector<float> output(BLOCK_FRAMES * CHANNELS);
	const Adagio::ProcessAudioResult full = processor.ProcessAudio(output.data(), output.size(), true);
	CHECK(full.FramesConsumed == BLOCK_FRAMES);
	CHECK_FALSE(full.Drained);

	const Adagio::ProcessAudioResult last = processor.ProcessAudio(output.data(), output.size(), true);
	CHECK(last.FramesConsumed == BLOCK_FRAMES / 2);
	CHECK(last.Drained);
}
