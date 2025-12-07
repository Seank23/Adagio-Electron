#include "PlaybackService.h"
#include "../Core/AudioDecoder.h"
#include "../Core/MessageQueue.h"

#include <algorithm>

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
		ma_device_uninit(&m_PlaybackDevice);
	}

    int PlaybackService::Init(std::shared_ptr<AudioDecoder> decoder)
	{
		m_PlaybackDevice = ma_device();
		m_Decoder = decoder;

		m_AudioSource = m_Decoder->GetAudioSource();

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
		ma_device_stop(&m_PlaybackDevice);
		ma_device_uninit(&m_PlaybackDevice);
		m_Decoder->Clear();
	}

	void PlaybackService::PlayAudio()
	{
		ma_device_start(&m_PlaybackDevice);
		m_Decoder->SetFeederState(FeederState::RUNNING);
	}

    void PlaybackService::PauseAudio()
    {
        ma_device_stop(&m_PlaybackDevice);
		m_Decoder->SetFeederState(FeederState::STOPPED);
    }

	void PlaybackService::StopAudio()
	{
		ma_device_stop(&m_PlaybackDevice);
		m_CurrentPlaybackFrame = 0;
		m_Decoder->ResetAudio();
	}

	void PlaybackService::SetVolume(float volume)
	{
		m_Volume = std::clamp(volume, 0.0f, 1.0f);
	}

	void PlaybackService::OnAudioCallback(float* outBuffer, ma_uint32 framesToRead)
	{
		const uint32_t samplesRequested = framesToRead * m_Decoder->GetAudioSource()->Channels;
		RingBuffer<float>* playbackBuffer = m_Decoder->GetBuffer("Playback");
		uint32_t samplesRead = playbackBuffer->Read(outBuffer, samplesRequested);
		if (samplesRead < samplesRequested)
			std::memset(outBuffer + samplesRead, 0, (samplesRequested - samplesRead) * sizeof(float));

		float volume = m_Volume.load(std::memory_order_relaxed);
		for (size_t i = 0; i < samplesRequested; i++)
			outBuffer[i] *= volume;

		uint64_t currentFrame = m_CurrentPlaybackFrame.load(std::memory_order_relaxed);
		currentFrame += framesToRead;
		m_CurrentPlaybackFrame.store(currentFrame);

		int updateCounter = m_PlaybackUpdateCounter.load(std::memory_order_relaxed);
		if (updateCounter >= 4)
		{
			if (currentFrame >= m_AudioSource->SamplesPerChannel)
			{
				MessageQueue::Instance().Push("{\"type\":\"endOfPlay\"}");
				return;
			}
			MessageQueue::Instance().Push(std::string("{\"type\":\"position\",\"value\":") + std::to_string(currentFrame) + "}");
			m_PlaybackUpdateCounter.store(0);
		}
		else
		{
			m_PlaybackUpdateCounter.store(updateCounter + 1);
		}
	}
}
