#include "PlaybackService.h"
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
		: m_CurrentState(PlayState::NOT_STARTED)
	{
	}

	PlaybackService::~PlaybackService()
	{
		ma_device_uninit(&m_PlaybackDevice);
	}

    int PlaybackService::Init(std::shared_ptr<AudioDecoder> decoder)
	{
		m_CurrentState = PlayState::NOT_STARTED;
		m_PlaybackDevice = ma_device();
		m_Decoder = decoder;

		m_AudioSource = m_Decoder->GetAudioSource();

		m_TimeProcessor = std::make_unique<TimeProcessor>();
		m_TimeProcessor->Init(m_AudioSource->SampleRate, m_AudioSource->Channels, m_Decoder->GetBuffer("Playback"));

		ma_device_config deviceConfig = ma_device_config_init(ma_device_type_playback);
		deviceConfig.playback.format = ma_format_f32;
		deviceConfig.playback.channels = m_AudioSource->Channels;
		deviceConfig.sampleRate = m_AudioSource->SampleRate;
		deviceConfig.dataCallback = DataCallback;
        deviceConfig.pUserData = this;

		if (ma_device_init(NULL, &deviceConfig, &m_PlaybackDevice) != MA_SUCCESS)
			return -1;
		return 1;
	}

	void PlaybackService::Reset()
	{
		m_CurrentState = PlayState::NOT_STARTED;
		m_CurrentPlaybackFrame = 0;
		ma_device_stop(&m_PlaybackDevice);
		ma_device_uninit(&m_PlaybackDevice);
		m_Decoder->Clear();
	}

	void PlaybackService::PlayAudio()
	{
		m_CurrentState = PlayState::PLAYING;
		ma_device_start(&m_PlaybackDevice);
		m_Decoder->SetFeederState(FeederState::RUNNING);
	}

    void PlaybackService::PauseAudio()
    {
		m_CurrentState = PlayState::PAUSED;
        ma_device_stop(&m_PlaybackDevice);
		m_Decoder->SetFeederState(FeederState::STOPPED);
    }

	void PlaybackService::StopAudio()
	{
		m_CurrentState = PlayState::STOPPED;
		ma_device_stop(&m_PlaybackDevice);
		m_CurrentPlaybackFrame = 0;
		m_Decoder->ResetAudio();
	}

	void PlaybackService::SetVolume(float volume)
	{
		m_Volume = std::clamp(volume, 0.0f, 1.0f);
	}

	void PlaybackService::SetSpeed(float speed)
	{
		m_TimeProcessor->SetSpeed(speed);
	}

	void PlaybackService::SeekToSample(uint64_t frame)
	{
		ma_device_stop(&m_PlaybackDevice);
		m_Decoder->SetFeederState(FeederState::STOPPED);
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		m_Decoder->SeekToSample(frame);
		m_CurrentPlaybackFrame = frame;
		if (m_CurrentState == PlayState::PLAYING)
		{
			ma_device_start(&m_PlaybackDevice);
			m_Decoder->SetFeederState(FeederState::RUNNING);
		}
	}

	void PlaybackService::OnAudioCallback(float* outBuffer, ma_uint32 framesToRead)
	{
		const uint32_t channels = m_Decoder->GetAudioSource()->Channels;
		const uint32_t samplesRequested = framesToRead * channels;

		size_t framesConsumed = m_TimeProcessor->ProcessAudio(outBuffer, samplesRequested);

		// Apply volume
		float volume = m_Volume.load(std::memory_order_relaxed);
		for (size_t i = 0; i < samplesRequested; i++)
			outBuffer[i] *= volume;

		// Playback tracking
		m_CurrentPlaybackFrame.fetch_add(framesConsumed, std::memory_order_release);
		uint64_t playbackFrame = m_CurrentPlaybackFrame.load();

		double seconds = (double)playbackFrame / (double)m_AudioSource->SampleRate;

		m_Decoder->SetPlaybackTime(seconds);
		m_Decoder->SetLastPlaybackFrameTimestamp(std::chrono::high_resolution_clock::now().time_since_epoch().count() / 1e9);

		// UI updates
		int updateCounter = m_PlaybackUpdateCounter.load(std::memory_order_relaxed);
		if (updateCounter >= 4)
		{
			if (playbackFrame >= m_AudioSource->SamplesPerChannel)
			{
				MessageQueue::Instance().Push("{\"type\":\"endOfPlay\"}");
				return;
			}

			MessageQueue::Instance().Push(
				std::string("{\"type\":\"position\",\"value\":") + std::to_string(seconds) + "}"
			);

			m_PlaybackUpdateCounter.store(0);
		}
		else
		{
			m_PlaybackUpdateCounter.store(updateCounter + 1);
		}
	}
}
