#include "AudioDecoder.h"
#include "MessageQueue.h"
#include "../IO/AudioData.h"

#include <algorithm>
#include <iostream>

namespace Adagio
{
	AudioDecoder::AudioDecoder()
		: m_FramesPerChunk(1024), m_SamplesPerChunk(0)
	{
	}

	AudioDecoder::~AudioDecoder()
	{
		Clear();
	}

	void AudioDecoder::Init(std::shared_ptr<AudioData> audioData)
	{
		Clear();
		m_AudioSource = audioData;

		const int channels = audioData->Channels;
		const size_t frames = static_cast<size_t>(audioData->SamplesPerChannel);

		m_FeederData.resize(frames * static_cast<size_t>(channels));
		for (size_t i = 0; i < frames; ++i)
		{
			for (int ch = 0; ch < channels; ++ch)
				m_FeederData[i * channels + ch] = audioData->PCMData[ch][i];
		}

		m_SamplesPerChunk = m_FramesPerChunk * static_cast<size_t>(channels);

		m_FeederState.store(FeederState::Stopped, std::memory_order_release);
		m_FeederPosition.store(0, std::memory_order_release);
		m_SeekTargetSample.store(0, std::memory_order_release);
		m_SeekGeneration.store(0, std::memory_order_release);
		m_FeederGeneration.store(0, std::memory_order_release);
		LaunchFeeder();
	}

	void AudioDecoder::LaunchFeeder()
	{
		m_FeederThread = std::thread([this]()
		{
			const size_t totalSamples = m_FeederData.size();
			const int channels = m_AudioSource ? m_AudioSource->Channels : 1;
			uint32_t seenGeneration = m_SeekGeneration.load(std::memory_order_acquire);

			while (m_FeederState.load(std::memory_order_acquire) != FeederState::Terminated)
			{
				// Adopt a pending seek before anything else, whether or not we are
				// running, then republish the generation so the audio callback knows
				// the write side has moved and it may drop what it still holds.
				const uint32_t generation = m_SeekGeneration.load(std::memory_order_acquire);
				if (generation != seenGeneration)
				{
					uint64_t target = m_SeekTargetSample.load(std::memory_order_acquire) * static_cast<uint64_t>(channels);
					if (target > totalSamples)
						target = totalSamples;
					m_FeederPosition.store(target, std::memory_order_release);
					seenGeneration = generation;
					m_FeederGeneration.store(generation, std::memory_order_release);
				}

				const uint64_t pos = m_FeederPosition.load(std::memory_order_acquire);
				if (pos < totalSamples)
				{
					if (m_FeederState.load(std::memory_order_acquire) == FeederState::Running)
					{
						const size_t samplesRemaining = totalSamples - static_cast<size_t>(pos);
						const size_t toWrite = std::min(samplesRemaining, m_SamplesPerChunk);

						const float* chunk = m_FeederData.data() + pos;
						bool shouldSleep = false;
						size_t minWritten = SIZE_MAX;
						for (auto& [name, buffer] : m_Buffers)
						{
							const size_t written = buffer->Write(chunk, toWrite);
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
					else
					{
						std::this_thread::sleep_for(std::chrono::milliseconds(10));
					}
				}
				else
				{
					FeederState expected = FeederState::Running;
					m_FeederState.compare_exchange_strong(expected, FeederState::Stopped,
						std::memory_order_acq_rel, std::memory_order_acquire);
					std::this_thread::sleep_for(std::chrono::milliseconds(10));
				}
			}
		});
	}

	void AudioDecoder::AddBuffer(const std::string& bufferName, float durationSeconds)
	{
		const size_t capacity = static_cast<size_t>(m_AudioSource->SampleRate * m_AudioSource->Channels * durationSeconds);
		m_Buffers[bufferName] = std::make_unique<RingBuffer<float>>(capacity);
	}

	void AudioDecoder::RequestSeek(uint64_t sample)
	{
		m_SeekTargetSample.store(sample, std::memory_order_release);
		m_SeekGeneration.fetch_add(1, std::memory_order_acq_rel);
	}

	void AudioDecoder::ResetAudio()
	{
		m_FeederState.store(FeederState::Stopped, std::memory_order_release);
		RequestSeek(0);
		SetPlaybackTime(0.0);
		SetLastPlaybackFrameTimestamp(0.0);
	}

	void AudioDecoder::Clear()
	{
		m_FeederState.store(FeederState::Terminated, std::memory_order_release);
		if (m_FeederThread.joinable())
			m_FeederThread.join();

		m_Buffers.clear();
		m_FeederData = kfr::univector<float>();
		m_AudioSource.reset();
		m_FeederPosition.store(0, std::memory_order_release);
		SetPlaybackTime(0.0);
		SetLastPlaybackFrameTimestamp(0.0);
	}

	RingBuffer<float>* AudioDecoder::GetBuffer(const std::string& bufferName)
	{
		const auto it = m_Buffers.find(bufferName);
		return it == m_Buffers.end() ? nullptr : it->second.get();
	}
}
