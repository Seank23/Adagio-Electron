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

		std::atomic<float> m_Volume{ 1.0f };
	};
}

