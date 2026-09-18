#include "Application.h"
#include "AudioDecoder.h"
#include "CommandQueue.h"
#include "MessageQueue.h"
#include "../Analysis/AnalysisService.h"
#include "../Debug/Instrumentation.h"
#include "../IO/AudioData.h"
#include "../IO/FileIOService.h"
#include "../IO/PlaybackService.h"
#include "../IO/WaveformBuilder.h"

#include <nlohmann/json.hpp>

#include <chrono>
#include <thread>

namespace Adagio
{
	namespace
	{
		void PushEvent(const nlohmann::json& event)
		{
			MessageQueue::GetInstance().Push(event.dump());
		}

		void PushError(const std::string& message)
		{
			PushEvent({ {"type", "error"}, {"value", message} });
		}

		void PushInfo(const std::string& message)
		{
			PushEvent({ {"type", "info"}, {"value", message} });
		}
	}

	Application::Application()
	{
		ADAGIO_PROFILE_BEGIN_SESSION("Application", "Application_Profile.json");
		m_AudioData = std::make_shared<AudioData>();
		m_AudioDecoder = std::make_shared<AudioDecoder>();
		m_FileIOService = std::make_unique<FileIOService>();
		m_PlaybackService = std::make_unique<PlaybackService>();
		m_AnalysisService = std::make_unique<AnalysisService>();
	}

	Application::~Application()
	{
		Shutdown();
		ADAGIO_PROFILE_END_SESSION();
	}

	void Application::Run()
	{
		m_Running.store(true, std::memory_order_release);
		while (m_Running.load(std::memory_order_acquire))
		{
			ProcessCommands();
			std::this_thread::sleep_for(std::chrono::milliseconds(2));
		}
		Shutdown();
	}

	void Application::Shutdown()
	{
		m_Running.store(false, std::memory_order_release);
		if (m_State.load(std::memory_order_acquire) != TransportState::Empty)
			ClearAudio();
		m_State.store(TransportState::Empty, std::memory_order_release);
	}

	void Application::ProcessCommands()
	{
		Command cmd;
		while (CommandQueue::GetInstance().Pop(cmd))
			HandleCommand(cmd);
	}

	void Application::HandleCommand(const Command& cmd)
	{
		// Bumped on the way out, refusals included, so /status can be polled for
		// "this command has been dealt with".
		struct Counted
		{
			std::atomic<uint64_t>& Counter;
			~Counted() { Counter.fetch_add(1, std::memory_order_acq_rel); }
		} counted{ m_CommandsHandled };

		const TransportState state = m_State.load(std::memory_order_acquire);

		// One gate for every command. Anything the current state cannot serve is
		// answered with an error rather than run against half-built services.
		if (!IsCommandLegal(state, cmd.Type))
		{
			PushEvent({
				{"type", "error"},
				{"value", RejectionReason(state, cmd.Type)},
				{"command", ToString(cmd.Type)},
				{"state", ToString(state)}
			});
			return;
		}

		switch (cmd.Type)
		{
		case CommandType::Load:
		{
			if (state != TransportState::Empty)
				ClearAudio();

			m_State.store(TransportState::Loading, std::memory_order_release);
			std::string error;
			if (LoadAudio(cmd.StrValue, error))
			{
				m_State.store(TransportState::Ready, std::memory_order_release);
				PushInfo("Audio file loaded successfully.");
			}
			else
			{
				ClearAudio();
				m_State.store(TransportState::Empty, std::memory_order_release);
				PushError(error);
				PushEvent({ {"type", "fileClosed"} });
			}
			break;
		}
		case CommandType::Play:
			m_PlaybackService->PlayAudio();
			m_AnalysisService->StartAnalysis();
			m_State.store(TransportState::Playing, std::memory_order_release);
			PushInfo("Playback started");
			break;
		case CommandType::Pause:
			m_PlaybackService->PauseAudio();
			m_AnalysisService->StopAnalysis();
			m_State.store(TransportState::Paused, std::memory_order_release);
			PushInfo("Playback paused");
			break;
		case CommandType::Stop:
			m_PlaybackService->StopAudio();
			m_AnalysisService->StopAnalysis();
			m_State.store(TransportState::Ready, std::memory_order_release);
			PushInfo("Playback stopped");
			break;
		case CommandType::Clear:
			ClearAudio();
			m_State.store(TransportState::Empty, std::memory_order_release);
			PushEvent({ {"type", "fileClosed"} });
			break;
		case CommandType::Seek:
		{
			const double duration = m_Duration.load(std::memory_order_acquire);
			double seconds = static_cast<double>(cmd.Value);
			seconds = std::clamp(seconds, 0.0, duration);
			m_PlaybackService->SeekToSample(static_cast<uint64_t>(seconds * m_AudioData->SampleRate));
			break;
		}
		case CommandType::SetVolume:
			m_PlaybackService->SetVolume(cmd.Value);
			break;
		case CommandType::SetSpeed:
			m_PlaybackService->SetSpeed(cmd.Value);
			break;
		case CommandType::AnalyseFrame:
			m_AnalysisService->RequestCurrentFrameAnalysis();
			break;
		case CommandType::Status:
			MessageQueue::GetInstance().Push(GetStatusJson());
			break;
		case CommandType::Shutdown:
			PushInfo("Engine shutting down.");
			m_Running.store(false, std::memory_order_release);
			break;
		}
	}

	bool Application::LoadAudio(const std::string& filePath, std::string& outError)
	{
		if (filePath.empty())
		{
			outError = "No file path was given.";
			return false;
		}

		const FileFormat format = FileIOService::FormatFromPath(filePath);
		if (format == FileFormat::Unknown)
		{
			outError = "Unsupported audio format. Supported: .wav, .mp3, .flac.";
			return false;
		}

		try
		{
			m_FileIOService->LoadAudio(filePath, format, *m_AudioData);
		}
		catch (const std::exception& e)
		{
			outError = e.what();
			return false;
		}
		catch (...)
		{
			outError = "Failed to load audio file.";
			return false;
		}

		// Nothing downstream may index PCMData before this holds: the waveform
		// builder and the feeder both read PCMData[0] from two threads.
		if (m_AudioData->Channels <= 0 || m_AudioData->SamplesPerChannel <= 0 || m_AudioData->PCMData.empty())
		{
			outError = "The file decoded to no audio.";
			return false;
		}

		m_Duration.store(static_cast<double>(m_AudioData->Duration), std::memory_order_release);

		WaveformBuilder waveformBuilder;
		std::thread waveformThread([&]()
		{
			waveformBuilder.BuildWaveform(m_AudioData);
			std::string json = "{\"type\":\"waveformData\",\"value\":[";
			auto& resolutions = waveformBuilder.GetAvailableResolutions();
			for (size_t i = 0; i < resolutions.size(); i++)
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
			MessageQueue::GetInstance().Push(json);
		});

		m_AudioDecoder->Init(m_AudioData);
		m_AudioDecoder->AddBuffer("Playback", 5.0f);

		if (m_PlaybackService->Init(m_AudioDecoder) < 0)
		{
			waveformThread.join();
			outError = "Could not open an audio output device.";
			return false;
		}
		m_AnalysisService->Init(m_AudioDecoder, AnalysisParams{ 8000, 4096 }, m_PlaybackService.get());

		PushEvent({ {"type", "fileLoaded"}, {"value", { {"duration", m_AudioData->Duration} }} });
		waveformThread.join();
		return true;
	}

	void Application::ClearAudio()
	{
		try
		{
			m_AnalysisService->Reset();
			m_PlaybackService->Reset();
			m_AudioDecoder->Clear();
			m_AudioData->Clear();
			m_Duration.store(0.0, std::memory_order_release);
		}
		catch (...)
		{
			PushError("Failed to release the loaded file cleanly.");
		}
	}

	std::string Application::GetStatusJson() const
	{
		const TransportState state = m_State.load(std::memory_order_acquire);
		const nlohmann::json status = {
			{"type", "status"},
			{"value", {
				{"state", ToString(state)},
				{"duration", m_Duration.load(std::memory_order_acquire)},
				{"position", m_PlaybackService->GetPositionSeconds()},
				{"speed", m_PlaybackService->GetSpeed()},
				{"volume", m_PlaybackService->GetVolume()},
				{"commandsHandled", m_CommandsHandled.load(std::memory_order_acquire)}
			}}
		};
		return status.dump();
	}
}
