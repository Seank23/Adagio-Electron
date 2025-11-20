#pragma once
#include <memory>
#include <string>

namespace Adagio
{
    enum PlayState;
	class AudioData;
    class AudioDecoder;
	class FileIOService;
	class PlaybackService;

	class Application
	{
	public:
		Application();
		virtual ~Application();

        int LoadAudio(std::string filePath);
        int ClearAudio();

        void UpdateAudioState(PlayState state);
        PlayState GetAudioState() { return m_AudioState; }

        float GetPlaybackSampleRate();
        float GetAudioDuration();

	private:
		std::shared_ptr<AudioDecoder> m_AudioDecoder;
        std::unique_ptr<FileIOService> m_FileIOService;
		std::unique_ptr<PlaybackService> m_PlaybackService;

        std::shared_ptr<AudioData> m_AudioData;
        bool m_AudioLoaded = false;

        PlayState m_AudioState;
	};
}
