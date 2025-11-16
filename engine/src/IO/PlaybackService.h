#pragma once

#include "AudioData.h"

#include <miniaudio.h>

namespace Adagio
{
	class PlaybackService
	{
	public:
		PlaybackService()
			: m_AudioData(nullptr), m_PlaybackDevice(ma_device()), m_AudioBuffer(ma_audio_buffer())
		{

		}
		~PlaybackService();

        int InitAudio(AudioData* audioData);
		void PlayAudio();
        void PauseAudio();
        void StopAudio();

        inline void SetCurrentSample(uint64_t sample) { m_AudioBuffer.ref.cursor = sample; }
		inline uint64_t GetCurrentSample() { return m_AudioBuffer.ref.cursor; }
		inline uint64_t GetSampleCount() { return m_AudioBuffer.ref.sizeInFrames; }

	private:
		AudioData* m_AudioData;
		std::vector<float> m_InterleavedStream;
		ma_device m_PlaybackDevice;
		ma_audio_buffer m_AudioBuffer;
	};
}

