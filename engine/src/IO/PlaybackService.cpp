#include "PlaybackService.h"
#include "../Buffers/RingBuffer.h"
#include "../Core/AudioDecoder.h"
#include "../Core/MessageQueue.h"
#include "../Core/TimeProcessor.h"

#include <algorithm>
#include <chrono>
#include <iostream>

#define MA_NO_WAV
#define MA_NO_MP3
#define MA_NO_FLAC
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

void DataCallback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
	Adagio::PlaybackService* self = reinterpret_cast<Adagio::PlaybackService*>(pDevice->pUserData);
	if (!self)
	{
		std::memset(pOutput, 0, frameCount * pDevice->playback.channels * ma_get_bytes_per_sample(pDevice->playback.format));
		return;
	}
	self->OnAudioCallback(reinterpret_cast<float*>(pOutput), frameCount);
}

namespace Adagio
{
	PlaybackService::PlaybackService()
	{
	}

	PlaybackService::~PlaybackService()
	{
		UninitDevice();
	}

	void PlaybackService::UninitDevice()
	{
		if (!m_DeviceInitialised)
			return;
		ma_device_stop(&m_PlaybackDevice);
		ma_device_uninit(&m_PlaybackDevice);
		m_DeviceInitialised = false;
	}

	int PlaybackService::Init(std::shared_ptr<AudioDecoder> decoder)
	{
		UninitDevice();

		m_Decoder = decoder;
		m_PlaybackBuffer = m_Decoder->GetBuffer("Playback");
		m_AudioSource = m_Decoder->GetAudioSource();
		m_CurrentPlaybackFrame.store(0, std::memory_order_release);
		m_PlaybackUpdateCounter.store(0, std::memory_order_release);
		m_EndReported.store(false, std::memory_order_release);
		m_SeekGenerationSeen = m_Decoder->GetSeekGeneration();

		m_TimeProcessor = std::make_unique<TimeProcessor>();
		m_TimeProcessor->Init(static_cast<int>(m_AudioSource->SampleRate), m_AudioSource->Channels, m_PlaybackBuffer);

		ma_device_config deviceConfig = ma_device_config_init(ma_device_type_playback);
		deviceConfig.playback.format = ma_format_f32;
		deviceConfig.playback.channels = m_AudioSource->Channels;
		deviceConfig.sampleRate = static_cast<ma_uint32>(m_AudioSource->SampleRate);
		deviceConfig.dataCallback = DataCallback;
		deviceConfig.pUserData = this;

		if (ma_device_init(NULL, &deviceConfig, &m_PlaybackDevice) != MA_SUCCESS)
		{
			m_TimeProcessor.reset();
			return -1;
		}
		m_DeviceInitialised = true;
		return 1;
	}

	void PlaybackService::Reset()
	{
		UninitDevice();
		m_CurrentPlaybackFrame.store(0, std::memory_order_release);
		m_EndReported.store(false, std::memory_order_release);
		if (m_TimeProcessor)
			m_TimeProcessor->Reset();
		m_TimeProcessor.reset();
		m_PlaybackBuffer = nullptr;
		if (m_Decoder)
			m_Decoder->Clear();
		m_Decoder.reset();
		m_AudioSource.reset();
	}

	void PlaybackService::PlayAudio()
	{
		if (!m_DeviceInitialised)
			return;
		m_EndReported.store(false, std::memory_order_release);
		ma_device_start(&m_PlaybackDevice);
		m_Decoder->SetFeederState(FeederState::Running);
	}

	void PlaybackService::PauseAudio()
	{
		if (!m_DeviceInitialised)
			return;
		ma_device_stop(&m_PlaybackDevice);
		m_Decoder->SetFeederState(FeederState::Stopped);
	}

	void PlaybackService::StopAudio()
	{
		if (!m_DeviceInitialised)
			return;
		ma_device_stop(&m_PlaybackDevice);
		m_CurrentPlaybackFrame.store(0, std::memory_order_release);
		m_EndReported.store(false, std::memory_order_release);
		m_Decoder->ResetAudio();
	}

	void PlaybackService::SetVolume(float volume)
	{
		m_Volume.store(std::clamp(volume, 0.0f, 1.0f), std::memory_order_release);
	}

	void PlaybackService::SetSpeed(float speed)
	{
		if (m_TimeProcessor)
			m_TimeProcessor->SetSpeed(std::clamp(speed, 0.2f, 2.0f));
	}

	float PlaybackService::GetSpeed() const
	{
		return m_TimeProcessor ? static_cast<float>(m_TimeProcessor->GetSpeed()) : 1.0f;
	}

	double PlaybackService::GetPositionSeconds() const
	{
		if (!m_AudioSource || m_AudioSource->SampleRate <= 0.0f)
			return 0.0;
		return static_cast<double>(m_CurrentPlaybackFrame.load(std::memory_order_acquire)) / m_AudioSource->SampleRate;
	}

	void PlaybackService::SeekToSample(uint64_t sample)
	{
		if (!m_Decoder)
			return;

		// Seek before position, so a callback finishing the track can't pin the playhead at the end.
		m_Decoder->RequestSeek(sample);
		m_CurrentPlaybackFrame.store(sample, std::memory_order_release);
		if (m_AudioSource && m_AudioSource->SampleRate > 0.0f)
			m_Decoder->SetPlaybackTime(static_cast<double>(sample) / m_AudioSource->SampleRate);
	}

	void PlaybackService::OnAudioCallback(float* outBuffer, ma_uint32 framesToRead)
	{
		const uint32_t channels = static_cast<uint32_t>(m_AudioSource->Channels);
		const uint32_t samplesRequested = framesToRead * channels;

		const uint32_t generation = m_Decoder->GetSeekGeneration();
		if (generation != m_SeekGenerationSeen)
		{
			// No mark until the feeder has taken the seek; play silence rather than stale audio.
			if (!m_PlaybackBuffer || !m_PlaybackBuffer->DropToMark(generation))
			{
				std::memset(outBuffer, 0, samplesRequested * sizeof(float));
				return;
			}

			m_TimeProcessor->ResetStretcher();
			m_CurrentPlaybackFrame.store(m_Decoder->GetSeekTargetSample(), std::memory_order_release);
			m_EndReported.store(false, std::memory_order_release);
			m_SeekGenerationSeen = generation;
		}

		const bool sourceExhausted = m_Decoder->GetIsSourceExhausted(generation);
		const ProcessAudioResult result = m_TimeProcessor->ProcessAudio(outBuffer, samplesRequested, sourceExhausted);

		const float volume = m_Volume.load(std::memory_order_relaxed);
		for (size_t i = 0; i < samplesRequested; i++)
			outBuffer[i] *= volume;

		// Read before the generation check, so a racing seek fails either that check or the compare-exchange.
		uint64_t playheadBefore = m_CurrentPlaybackFrame.load(std::memory_order_acquire);
		const bool reachedEnd = result.Drained && m_Decoder->GetSeekGeneration() == generation;
		if (reachedEnd)
		{
			// Start-delay accounting leaves the counter short of the end, so snap to it.
			m_CurrentPlaybackFrame.compare_exchange_strong(playheadBefore, static_cast<uint64_t>(m_AudioSource->SamplesPerChannel),
				std::memory_order_acq_rel, std::memory_order_acquire);
		}
		else
		{
			m_CurrentPlaybackFrame.fetch_add(result.FramesConsumed, std::memory_order_release);
		}

		const uint64_t playbackFrame = m_CurrentPlaybackFrame.load(std::memory_order_acquire);
		const double seconds = static_cast<double>(playbackFrame) / static_cast<double>(m_AudioSource->SampleRate);

		m_Decoder->SetPlaybackTime(seconds);
		m_Decoder->SetLastPlaybackFrameTimestamp(std::chrono::high_resolution_clock::now().time_since_epoch().count() / 1e9);

		if (reachedEnd)
		{
			if (!m_EndReported.exchange(true, std::memory_order_acq_rel))
				MessageQueue::GetInstance().Push("{\"type\":\"endOfPlay\"}");
			return;
		}

		const int updateCounter = m_PlaybackUpdateCounter.load(std::memory_order_relaxed);
		if (updateCounter >= 4)
		{
			MessageQueue::GetInstance().Push(
				std::string("{\"type\":\"position\",\"value\":") + std::to_string(seconds) + "}"
			);
			m_PlaybackUpdateCounter.store(0, std::memory_order_relaxed);
		}
		else
		{
			m_PlaybackUpdateCounter.store(updateCounter + 1, std::memory_order_relaxed);
		}
	}
}
