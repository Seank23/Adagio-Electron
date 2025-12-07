#include "Application.h"
#include "AudioDecoder.h"
#include "../IO/AudioData.h"
#include "../IO/FileIOService.h"
#include "../IO/PlaybackService.h"
#include "../API/Utils.h"
#include "../Debug/Instrumentation.h"
#include "MessageQueue.h"

#include <filesystem>

namespace Adagio
{
	Application::Application()
    : m_AudioState(PlayState::NOT_STARTED)
	{
		ADAGIO_PROFILE_BEGIN_SESSION("Application", "Application_Profile.json");
        m_AudioData = std::make_shared<AudioData>();
        m_AudioDecoder = std::make_unique<AudioDecoder>();
		m_FileIOService = std::make_unique<FileIOService>();
        m_PlaybackService = std::make_unique<PlaybackService>();
	}

	Application::~Application()
	{
        ADAGIO_PROFILE_END_SESSION();
	}

    int Application::LoadAudio(std::string filePath)
    {
        try
        {
            std::filesystem::path path = filePath;
            auto extension = path.extension();
            if (extension == ".wav")
                m_FileIOService->LoadAudio(path.string(), FileFormat::WAV, *m_AudioData.get());
            else if (extension == ".mp3")
                m_FileIOService->LoadAudio(path.string(), FileFormat::MP3, *m_AudioData.get());
            else if (extension == ".flac")
                m_FileIOService->LoadAudio(path.string(), FileFormat::FLAC, *m_AudioData.get());
        } catch (...)
        {
            return -1;
        }
        if (m_AudioData != nullptr)
        {
            m_AudioLoaded = true;
			m_AudioDecoder->Init(m_AudioData);
            m_AudioDecoder->AddBuffer("Playback", 5.0f);
			m_PlaybackService->Init(m_AudioDecoder);
            MessageQueue::Instance().Push("{\"type\":\"fileLoaded\",\"value\":{\"totalSamples\":" + std::to_string(m_AudioData->SamplesPerChannel) + "} }");
        }
        return 1;
    }

    int Application::ClearAudio()
    {
        try
        {
			m_PlaybackService->Reset();
            m_AudioData->Clear();
            m_AudioLoaded = false;
        } catch (...)
        {
            return -1;
        }
        return 1;
    }

    void Application::UpdateAudioState(PlayState state)
    {
        m_AudioState = state;
        switch (m_AudioState)
        {
        case PlayState::NOT_STARTED:
            break;
        case PlayState::PLAYING:
            m_PlaybackService->PlayAudio();
            break;
        case PlayState::PAUSED:
            m_PlaybackService->PauseAudio();
            break;
        case PlayState::STOPPED:
            m_PlaybackService->StopAudio();
            break;
        }
    }

    void Application::SetVolume(float volume) { m_PlaybackService->SetVolume(volume); }
}
