#include "PcmFeeder.h"
#include "../IO/AudioData.h"

#include <algorithm>
#include <iostream>

namespace Adagio
{
	PcmFeeder::PcmFeeder()
		: m_FramesPerChunk(1024), m_SamplesPerChunk(0)
	{
	}

	PcmFeeder::~PcmFeeder()
	{
		Clear();
	}

	void PcmFeeder::Init(std::shared_ptr<AudioData> audioData)
	{
		Clear();
		m_AudioSource = audioData;

		const size_t channels = (size_t)audioData->Channels; 

		m_SamplesPerChunk = m_FramesPerChunk * channels;
		m_TotalSamples.store((size_t)audioData->SamplesPerChannel * channels, std::memory_order_release);
		m_FeederState.store(FeederState::Stopped, std::memory_order_release);
		m_FeederPosition.store(0, std::memory_order_release);
		m_SeekTargetSample.store(0, std::memory_order_release);
		m_SeekGeneration.store(0, std::memory_order_release);
		m_FeederGeneration.store(0, std::memory_order_release);
		LaunchFeeder();
	}

	void PcmFeeder::LaunchFeeder()
	{
		m_FeederThread = std::thread([this]()
		{
			kfr::univector<float> chunkBuffer(m_SamplesPerChunk);
			const size_t totalSamples = m_TotalSamples.load(std::memory_order_acquire);
			const int channels = m_AudioSource ? m_AudioSource->Channels : 1;
			uint32_t seenGeneration = m_SeekGeneration.load(std::memory_order_acquire);

			while (m_FeederState.load(std::memory_order_acquire) != FeederState::Terminated)
			{
				// Mark before moving, so readers drop only what was written before the seek.
				const uint32_t generation = m_SeekGeneration.load(std::memory_order_acquire);
				if (generation != seenGeneration)
				{
					uint64_t target = m_SeekTargetSample.load(std::memory_order_acquire) * static_cast<uint64_t>(channels);
					if (target > totalSamples)
						target = totalSamples;
					for (auto& [name, buffer] : m_Buffers)
						buffer->MarkWritePosition(generation);
					m_FeederPosition.store(target, std::memory_order_release);
					seenGeneration = generation;
					m_FeederGeneration.store(generation, std::memory_order_release);
				}

				// Leave the state alone at end of file: a seek back needs the feeder still Running.
				const uint64_t pos = m_FeederPosition.load(std::memory_order_acquire);
				if (pos >= totalSamples || m_FeederState.load(std::memory_order_acquire) != FeederState::Running)
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(10));
					continue;
				}

				const size_t samplesRemaining = totalSamples - static_cast<size_t>(pos);
				const size_t toWrite = std::min(samplesRemaining, m_SamplesPerChunk);

				for (size_t i = 0; i < toWrite; ++i)
				{
					const uint64_t sample = pos + i;
					chunkBuffer[i] = m_AudioSource->PCMData[sample % channels][sample / channels];
				}
				bool shouldSleep = false;
				size_t minWritten = SIZE_MAX;
				for (auto& [name, buffer] : m_Buffers)
				{
					const size_t written = buffer->Write(chunkBuffer.data(), toWrite);
					minWritten = std::min(minWritten, written);
					if (written == 0)
						shouldSleep = true;
				}
				if (minWritten == SIZE_MAX)
					minWritten = 0;
				if (shouldSleep)
					std::this_thread::sleep_for(std::chrono::milliseconds(2));

				// Only advance if this iteration still owns the position. A
				// seek adopted mid-iteration must not be overwritten.
				uint64_t expected = pos;
				m_FeederPosition.compare_exchange_strong(expected, pos + minWritten,
					std::memory_order_acq_rel, std::memory_order_acquire);
			}
		});
	}

	void PcmFeeder::AddBuffer(const std::string& bufferName, float durationSeconds)
	{
		const size_t capacity = static_cast<size_t>(m_AudioSource->SampleRate * m_AudioSource->Channels * durationSeconds);
		m_Buffers[bufferName] = std::make_unique<RingBuffer<float>>(capacity);
	}

	void PcmFeeder::RequestSeek(uint64_t sample)
	{
		m_SeekTargetSample.store(sample, std::memory_order_release);
		m_SeekGeneration.fetch_add(1, std::memory_order_acq_rel);
	}

	bool PcmFeeder::GetIsSourceExhausted(uint32_t generation) const
	{
		// Generation first: the feeder publishes it after moving, so the position read next is current.
		return m_FeederGeneration.load(std::memory_order_acquire) == generation
			&& m_FeederPosition.load(std::memory_order_acquire) >= m_TotalSamples.load(std::memory_order_acquire);
	}

	void PcmFeeder::ResetAudio()
	{
		m_FeederState.store(FeederState::Stopped, std::memory_order_release);
		RequestSeek(0);
		SetPlaybackTime(0.0);
		SetLastPlaybackFrameTimestamp(0.0);
	}

	void PcmFeeder::Clear()
	{
		m_FeederState.store(FeederState::Terminated, std::memory_order_release);
		if (m_FeederThread.joinable())
			m_FeederThread.join();

		m_Buffers.clear();
		m_AudioSource.reset();
		m_FeederPosition.store(0, std::memory_order_release);
		m_TotalSamples.store(0, std::memory_order_release);
		SetPlaybackTime(0.0);
		SetLastPlaybackFrameTimestamp(0.0);
	}

	RingBuffer<float>* PcmFeeder::GetBuffer(const std::string& bufferName)
	{
		const auto it = m_Buffers.find(bufferName);
		return it == m_Buffers.end() ? nullptr : it->second.get();
	}
}
