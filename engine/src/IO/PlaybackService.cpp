#include "PlaybackService.h"

#define MA_NO_WAV
#define MA_NO_MP3
#define MA_NO_FLAC
#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>
#include <kfr/base.hpp>

#include "../Core/AudioUtils.h"

namespace Adagio
{
	PlaybackService::~PlaybackService()
	{
		ma_device_uninit(&m_PlaybackDevice);
	}

	void DataCallback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
	{
		ma_audio_buffer* pBuffer = (ma_audio_buffer*)pDevice->pUserData;
		ma_audio_buffer_read_pcm_frames(pBuffer, pOutput, frameCount, NULL);
		(void)pInput;
	}

    int PlaybackService::InitAudio(AudioData* audioData)
	{
        m_AudioData = audioData;
		m_InterleavedStream.clear();

		if (m_AudioData->Channels == 2)
			AudioUtils::InterleaveStereo(m_AudioData->PlaybackStream, &m_InterleavedStream);
		else
			AudioUtils::UnivectorToMono(m_AudioData->PlaybackStream[0], &m_InterleavedStream);

		ma_audio_buffer_config audioBufferConfig = ma_audio_buffer_config_init(ma_format_f32, m_AudioData->Channels, m_AudioData->PlaybackStream[0].size(), &m_InterleavedStream[0], NULL);
		ma_audio_buffer_init(&audioBufferConfig, &m_AudioBuffer);

		ma_device_config deviceConfig = ma_device_config_init(ma_device_type_playback);
		deviceConfig.playback.format = ma_format_f32;
		deviceConfig.playback.channels = m_AudioData->Channels;
		deviceConfig.sampleRate = m_AudioData->PlaybackSampleRate;
		deviceConfig.dataCallback = DataCallback;
        deviceConfig.pUserData = &m_AudioBuffer;

		if (ma_device_init(NULL, &deviceConfig, &m_PlaybackDevice) != MA_SUCCESS)
			return -1;
		return 1;
	}

	void PlaybackService::PlayAudio()
	{
		ma_device_start(&m_PlaybackDevice);
	}

    void PlaybackService::PauseAudio()
    {
        ma_device_stop(&m_PlaybackDevice);
    }

	void PlaybackService::StopAudio()
	{
		ma_device_stop(&m_PlaybackDevice);
        SetCurrentSample(0);
	}
}
