#pragma once
#include "AudioData.h"

#include <memory>
#include <atomic>
#include "miniaudio.h"

namespace Adagio
{
	class AudioDecoder;

	class PlaybackService
	{
	public:
		PlaybackService();
		~PlaybackService();

		void OnAudioCallback(float* outBuffer, ma_uint32 framesToRead);

        int Init(std::shared_ptr<AudioDecoder> decoder);
		void Reset();
		void PlayAudio();
        void PauseAudio();
        void StopAudio();
		void SetVolume(float volume);

	private:
		std::shared_ptr<AudioDecoder> m_Decoder;
		ma_device m_PlaybackDevice;

		std::shared_ptr<AudioData> m_AudioSource;
		std::atomic<float> m_Volume{ 1.0f };
		std::atomic<uint64_t> m_CurrentPlaybackFrame{ 0 };
		std::atomic<int> m_PlaybackUpdateCounter{ 0 };
	};
}

