#include "AudioDecoder.h"
#include "../IO/AudioData.h"
#include <iostream>
#include "MessageQueue.h"

namespace Adagio
{
	AudioDecoder::AudioDecoder()
		: m_FramesPerChunk(1024)
	{
	}

	AudioDecoder::~AudioDecoder()
	{
	}

	void AudioDecoder::Init(std::shared_ptr<AudioData> audioData)
	{
		m_AudioSource = audioData;
		if (audioData->Channels == 2)
			m_FeederData = kfr::interleave<float>(audioData->PCMData);
		else
			m_FeederData = audioData->PCMData[0];

		m_SamplesPerChunk = (int)m_FramesPerChunk * audioData->Channels;

		m_FeederState = FeederState::STOPPED;
		m_FeederPosition = 0;
		LaunchFeeder();
	}

	void AudioDecoder::LaunchFeeder()
	{
		m_FeederThread = std::thread([this]()
		{
			size_t totalSamples = m_FeederData.size();

			while (m_FeederState.load(std::memory_order_acquire) != FeederState::TERMINATED)
			{
				uint64_t pos = m_FeederPosition.load(std::memory_order_acquire);
				if (pos < totalSamples)
				{
					if (m_FeederState.load(std::memory_order_acquire) == FeederState::RUNNING)
					{
						size_t samplesRemaining = totalSamples - pos;
						size_t toWrite = std::min(samplesRemaining, m_SamplesPerChunk);

						const float* chunk = m_FeederData.data() + pos;
						bool shouldSleep = false;
						size_t minWritten = SIZE_MAX;
						for (auto& [name, buffer] : m_Buffers)
						{
							size_t written = buffer->Write(chunk, toWrite);
							minWritten = std::min(minWritten, written);
							if (written == 0) shouldSleep = true;
						}
						if (shouldSleep)
							std::this_thread::sleep_for(std::chrono::milliseconds(2));
						m_FeederPosition.store(pos + minWritten);
					}
					else
					{
						std::this_thread::sleep_for(std::chrono::milliseconds(10));
					}
				}
				else
				{
					m_FeederState.store(FeederState::STOPPED);
				}
			}
		});	
	}

	void AudioDecoder::AddBuffer(const std::string& bufferName, float durationSeconds)
	{
		size_t capacity = static_cast<size_t>(m_AudioSource->SampleRate * m_AudioSource->Channels * durationSeconds);
		std::unique_ptr<RingBuffer<float>> buffer = std::make_unique<RingBuffer<float>>(capacity);
		m_Buffers[bufferName] = std::move(buffer);
	}

	void AudioDecoder::SeekToSample(uint64_t sample)
	{
		uint64_t targetPosition = sample * m_AudioSource->Channels;
		if (targetPosition >= m_FeederData.size())
			targetPosition = m_FeederData.size();
		m_FeederPosition.store(targetPosition, std::memory_order_release);
		SetLastPlaybackFrameTimestamp(0.0);
		for (auto& [name, buffer] : m_Buffers)
			buffer->Clear();
	}

	void AudioDecoder::ResetAudio()
	{
		m_FeederState = FeederState::STOPPED;
		m_FeederPosition.store(0);
		SetLastPlaybackFrameTimestamp(0.0);
	}

	void AudioDecoder::Clear()
	{
		ResetAudio();
		m_FeederState = FeederState::TERMINATED;
		if (m_FeederThread.joinable())
			m_FeederThread.join();
		m_Buffers.clear();
	}

	RingBuffer<float>* AudioDecoder::GetBuffer(const std::string& bufferName)
	{
		return m_Buffers.at(bufferName).get();
	}
}