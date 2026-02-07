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
	class AnalysisService;

	class Application
	{
	public:
		Application();
		virtual ~Application();

		void Run();

	private:
		void ProcessCommands();
		int LoadAudio(std::string filePath);
		int ClearAudio();

		void UpdateAudioState(PlayState state);

		std::shared_ptr<AudioDecoder> m_AudioDecoder;
        std::unique_ptr<FileIOService> m_FileIOService;
		std::unique_ptr<PlaybackService> m_PlaybackService;
		std::unique_ptr<AnalysisService> m_AnalysisService;

        std::shared_ptr<AudioData> m_AudioData;
        bool m_AudioLoaded = false;

        PlayState m_AudioState;
	};
}
