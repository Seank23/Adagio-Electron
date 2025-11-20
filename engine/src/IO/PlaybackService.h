#pragma once
#include "AudioData.h"

#include <memory>
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

	private:
		std::shared_ptr<AudioDecoder> m_Decoder;
		ma_device m_PlaybackDevice;
	};
}

