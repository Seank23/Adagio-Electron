#include "Application.h"
#include <filesystem>
#include "../Debug/Instrumentation.h"

#include "../IO/AudioData.h"
#include "AudioUtils.h"

namespace Adagio
{
	Application::Application()
    : m_AudioState(PlayState::NOT_STARTED)
	{
		ADAGIO_PROFILE_BEGIN_SESSION("Application", "Application_Profile.json");
        m_AudioData = new AudioData;
        // AudioData wavAudio;
        // m_FileIOService.LoadAudio("C:\\Users\\seank\\Documents\\Audacity\\Guitar Freq Analysis.wav", FileFormat::WAV, m_AudioData);

		/*AudioData mp3Audio;
		m_FileIOService.LoadAudio("C:\\Users\\seank\\Documents\\Music\\Music Analysis\\Jazz\\Dont Fence Me In - Bing Crosby  The Andrews Sisters.mp3", FileFormat::MP3, mp3Audio);*/

        //AnalysisParams analysisParams;
        //analysisParams.SampleRate = 8000.0f;
        //m_AnalysisService.Init(wavAudio, analysisParams);

        // m_PlaybackService.InitAudio(wavAudio);
        // m_PlaybackService.PlayAudio();

        //uint64_t sampleCount = m_PlaybackService.GetSampleCount();
        //uint64_t currentSample = 0;
        // while (currentSample < sampleCount)
        // {
        // 	currentSample = m_PlaybackService.GetCurrentSample();
        // 	auto result = m_AnalysisService.AnalyseFrame(currentSample * (analysisParams.SampleRate / wavAudio.PlaybackSampleRate));
        // 	std::cout << m_PlaybackService.GetCurrentSample() << " - " << *std::max_element(result.begin(), result.end()) << '\n';
        // }
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
                m_FileIOService.LoadAudio(path.string(), FileFormat::WAV, *m_AudioData);
            else if (extension == ".mp3")
                m_FileIOService.LoadAudio(path.string(), FileFormat::MP3, *m_AudioData);
            else if (extension == ".flac")
                m_FileIOService.LoadAudio(path.string(), FileFormat::FLAC, *m_AudioData);
        } catch (...)
        {
            return -1;
        }
        if (m_AudioData != nullptr)
        {
            m_AudioLoaded = true;
            m_PlaybackService.InitAudio(m_AudioData);
        }
        return 1;
    }

    int Application::ClearAudio()
    {
        try
        {
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
            m_PlaybackService.PlayAudio();
            break;
        case PlayState::PAUSED:
            m_PlaybackService.PauseAudio();
            break;
        case PlayState::STOPPED:
            m_PlaybackService.StopAudio();
            break;
        }
    }

    std::vector<float> Application::ConstructWaveformData()
    {
        auto playbackStream = m_AudioData->PlaybackStream;
        std::vector<float> waveformData;
        int streamLength = playbackStream[0].size();
        for (int i = 0; i < streamLength; i++)
        {
            float waveformSample = (playbackStream[0][i] + playbackStream[1][i]) / 2.0f;
            waveformData.push_back(waveformSample);
        }
        return waveformData;
    }
}
