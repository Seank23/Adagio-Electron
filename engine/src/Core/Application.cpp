#include "Application.h"
#include "AudioDecoder.h"
#include "../IO/AudioData.h"
#include "../IO/FileIOService.h"
#include "../IO/PlaybackService.h"
#include "../API/Utils.h"
#include "../Debug/Instrumentation.h"
#include "MessageQueue.h"
#include "CommandQueue.h"
#include "../IO/WaveformBuilder.h"
#include "../Analysis/AnalysisService.h"


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
		m_AnalysisService = std::make_unique<AnalysisService>();
	}

	Application::~Application()
	{
        ADAGIO_PROFILE_END_SESSION();
	}

    void Application::Run()
    {
        while (true)
        {
            ProcessCommands();
			std::this_thread::sleep_for(std::chrono::milliseconds(2));
		}
    }

    void Application::ProcessCommands()
    {
        Command cmd;
        while (CommandQueue::Instance().Pop(cmd))
        {
            switch (cmd.Type)
            {
            case CommandType::Play:
                UpdateAudioState(PlayState::PLAYING);
                MessageQueue::Instance().Push("{\"type\":\"info\",\"value\":\"Playback started\"}");
                break;
            case CommandType::Pause:
                UpdateAudioState(PlayState::PAUSED);
                MessageQueue::Instance().Push("{\"type\":\"info\",\"value\":\"Playback paused\"}");
                break;
            case CommandType::Stop:
                UpdateAudioState(PlayState::STOPPED);
                MessageQueue::Instance().Push("{\"type\":\"info\",\"value\":\"Playback stopped\"}");
                break;
            case CommandType::Load:
                try
                {
                    int success = LoadAudio(cmd.StrValue);
                    if (success == -1)
                        MessageQueue::Instance().Push("{\"type\":\"error\",\"value\":\"Failed to load audio file.\"}");
                    else if (success == 1)
						MessageQueue::Instance().Push("{\"type\":\"info\",\"value\":\"Audio file loaded successfully.\"}");

                } catch (...)
                {
                    MessageQueue::Instance().Push("{\"type\":\"error\",\"value\":\"Failed to load audio file.\"}");
				}
                break;
			case CommandType::Clear:
                ClearAudio();
                MessageQueue::Instance().Push("{\"type\":\"fileClosed\"}");
				break;
            case CommandType::Seek:
                if (m_AudioLoaded)
                {
                    float seconds = cmd.Value;
                    if (seconds < 0.0f) seconds = 0.0f;
                    if (seconds > static_cast<float>(m_AudioData->Duration))
                        seconds = static_cast<float>(m_AudioData->Duration);
					uint64_t targetSample = static_cast<uint64_t>(seconds * m_AudioData->SampleRate);
                    m_PlaybackService->SeekToSample(targetSample);
                }
                break;
            case CommandType::SetVolume:
                m_PlaybackService->SetVolume(cmd.Value);
                break;
			case CommandType::AnalyseFrame:
                if (m_AudioLoaded)
                {
                    m_AnalysisService->RequestCurrentFrameAnalysis();
                }
				break;
            case CommandType::SetSpeed:
                if (m_AudioLoaded)
                {
                    float speed = cmd.Value;
                    if (speed < 0.2f) speed = 0.2f;
                    if (speed > 2.0f) speed = 2.0f;
                    m_PlaybackService->SetSpeed(speed);
                }
                break;
            }
        }
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
			WaveformBuilder waveformBuilder;
            std::thread waveformThread([&]()
            {
                waveformBuilder.BuildWaveform(m_AudioData);
                std::string json = "{\"type\":\"waveformData\",\"value\":[";
				auto& resolutions = waveformBuilder.GetAvailableResolutions();
                for (int i = 0; i < resolutions.size(); i++)
                {
                    const auto& data = waveformBuilder.GetWaveformData(resolutions[i]);
					json += "{\"resolution\":" + std::to_string(resolutions[i]) + ",\"peaks\":[";
                    for (size_t j = 0; j < data.size(); ++j)
                    {
                        json += std::to_string(data[j].Max);
                        if (j < data.size() - 1)
                            json += ",";
                    }
                    json += "]}";
                    if (i < resolutions.size() - 1)
                        json += ",";
                }
				json += "]}";
                MessageQueue::Instance().Push(json);
			});
			m_AudioDecoder->Init(m_AudioData);
            m_AudioDecoder->AddBuffer("Playback", 5.0f);
			m_PlaybackService->Init(m_AudioDecoder);
			m_AnalysisService->Init(m_AudioDecoder, AnalysisParams{ 8000, 2048 });
            MessageQueue::Instance().Push("{\"type\":\"fileLoaded\",\"value\":{\"duration\":" + std::to_string(m_AudioData->Duration) + "} }");
            waveformThread.join();
        }
        return 1;
    }

    int Application::ClearAudio()
    {
        try
        {
			m_PlaybackService->Reset();
			m_AnalysisService->Reset();
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
			m_AnalysisService->StartAnalysis();
            break;
        case PlayState::PAUSED:
            m_PlaybackService->PauseAudio();
			m_AnalysisService->StopAnalysis();
            break;
        case PlayState::STOPPED:
            m_PlaybackService->StopAudio();
			m_AnalysisService->StopAnalysis();
            break;
        }
    }
}
