// The feeder interleaves planar PCM one chunk at a time instead of keeping a
// whole interleaved copy of the file (F5). These cases pin that what reaches the
// ring buffer is exactly what the old copy held, sample for sample, for mono,
// stereo and 5.1, and again after a seek to an odd frame.
#include "../src/Core/PcmFeeder.h"
#include "../src/IO/AudioData.h"

#include <doctest/doctest.h>

#include <chrono>
#include <memory>
#include <thread>
#include <vector>

namespace
{
	constexpr int Frames = 500;

	// Every sample is distinct, so a swapped channel or a frame out of place shows.
	std::shared_ptr<Adagio::AudioData> MakeSource(int channels)
	{
		kfr::univector2d<float> pcm(channels, kfr::univector<float>(Frames));
		for (int ch = 0; ch < channels; ++ch)
			for (int frame = 0; frame < Frames; ++frame)
				pcm[ch][frame] = (float)(ch * 10000 + frame);

		auto source = std::make_shared<Adagio::AudioData>(pcm);
		source->SampleRate = 1000.0f;
		source->Channels = channels;
		source->SamplesPerChannel = Frames;
		source->Duration = (float)Frames / source->SampleRate;
		return source;
	}

	// What the interleaved copy held from fromFrame to the end of the source.
	std::vector<float> Interleaved(const Adagio::AudioData& source, int fromFrame)
	{
		std::vector<float> samples;
		for (int frame = fromFrame; frame < Frames; ++frame)
			for (int ch = 0; ch < source.Channels; ++ch)
				samples.push_back(source.PCMData[ch][frame]);
		return samples;
	}

	template <typename Predicate>
	bool WaitFor(Predicate predicate)
	{
		const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
		while (!predicate())
		{
			if (std::chrono::steady_clock::now() > deadline)
				return false;
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		return true;
	}

	std::vector<float> ReadAll(Adagio::RingBuffer<float>& buffer, size_t expected)
	{
		REQUIRE(WaitFor([&]() { return buffer.GetAvailableCount() >= expected; }));
		std::vector<float> samples(buffer.GetAvailableCount());
		buffer.Read(samples.data(), samples.size());
		return samples;
	}
}

TEST_CASE("the feeder writes the source interleaved, and again from an odd frame after a seek")
{
	for (int channels : { 1, 2, 6 })
	{
		CAPTURE(channels);
		const auto source = MakeSource(channels);

		Adagio::PcmFeeder feeder;
		feeder.Init(source);
		feeder.AddBuffer("Test", 5.0f);
		Adagio::RingBuffer<float>& buffer = *feeder.GetBuffer("Test");
		feeder.SetFeederState(Adagio::FeederState::Running);

		const std::vector<float> whole = Interleaved(*source, 0);
		CHECK(ReadAll(buffer, whole.size()) == whole);

		constexpr int seekFrame = 77;
		feeder.RequestSeek(seekFrame);
		const uint32_t generation = feeder.GetSeekGeneration();
		REQUIRE(WaitFor([&]() { return buffer.DropToMark(generation); }));

		const std::vector<float> tail = Interleaved(*source, seekFrame);
		CHECK(ReadAll(buffer, tail.size()) == tail);
		// The feeder advances its position just after the write the reader has seen.
		CHECK(WaitFor([&]() { return feeder.GetIsSourceExhausted(generation); }));
	}
}
