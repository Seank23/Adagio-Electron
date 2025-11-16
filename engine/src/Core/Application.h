#pragma once

#include "../IO/PlaybackService.h"
#include "../IO/FileIOService.h"
#include "../Analysis/AnalysisService.h"
#include "../API/Utils.h"

namespace Adagio
{
	class Application
	{
	public:
		Application();
		virtual ~Application();

        int LoadAudio(std::string filePath);
        int ClearAudio();

        void UpdateAudioState(PlayState state);
        PlayState GetAudioState() { return m_AudioState; }

        std::vector<float> ConstructWaveformData();
        float GetPlaybackSampleRate() { return m_AudioData->PlaybackSampleRate; }
        float GetAudioDuration() { return m_AudioData->Duration; }
        uint64_t GetAudioSampleCount() { return m_PlaybackService.GetSampleCount(); }
        uint64_t GetAudioCurrentSample() { return m_PlaybackService.GetCurrentSample(); }

        void SetPlaybackPosition(uint64_t sampleIndex) { m_PlaybackService.SetCurrentSample(sampleIndex); }

	private:
        FileIOService m_FileIOService;
		PlaybackService m_PlaybackService;
		AnalysisService m_AnalysisService;

        AudioData* m_AudioData;
        bool m_AudioLoaded = false;

        PlayState m_AudioState;
	};
}
