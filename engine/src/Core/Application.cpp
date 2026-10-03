#include "Application.h"
#include "PcmFeeder.h"
#include "BinaryFrame.h"
#include "CommandParser.h"
#include "CommandQueue.h"
#include "HighResolutionTimer.h"
#include "MessageQueue.h"
#include "Trace.h"
#include "../Analysis/AnalysisService.h"
#include "../Debug/Instrumentation.h"
#include "../IO/AudioData.h"
#include "../IO/FileIOService.h"
#include "../IO/PlaybackService.h"
#include "../IO/WaveformBuilder.h"
#include "TransportState.h"

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
			PushEvent({ {"type", Protocol::Event::Error}, {"value", message} });
		}

		void PushInfo(const std::string& message)
		{
			PushEvent({ {"type", Protocol::Event::Info}, {"value", message} });
		}

		CommandOutcome Failed(const std::string& error)
		{
			CommandOutcome outcome;
			outcome.Ok = false;
			outcome.Error = error;
			return outcome;
		}

		bool ChangesTransport(CommandType type)
		{
			switch (type)
			{
			case CommandType::Load:
			case CommandType::Play:
			case CommandType::Pause:
			case CommandType::Stop:
			case CommandType::Clear:
			case CommandType::Seek:
			case CommandType::StepFrame:
			case CommandType::SetVolume:
			case CommandType::SetSpeed:
				return true;
			default:
				return false;
			}
		}
	}

	Application::Application()
	{
		ADAGIO_PROFILE_BEGIN_SESSION("Application", "Application_Profile.json");
		m_AudioData = std::make_shared<AudioData>();
		m_PcmFeeder = std::make_shared<PcmFeeder>();
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
		constexpr auto positionInterval = std::chrono::milliseconds(33);
		HighResolutionTimer timer;
		auto lastPosition = std::chrono::steady_clock::now();
		while (m_Running.load(std::memory_order_acquire))
		{
			ProcessCommands();
			const auto now = std::chrono::steady_clock::now();
			if (now - lastPosition >= positionInterval)
			{
				PushEvent({ {"type", Protocol::Event::Position}, {"value", m_PlaybackService->GetPositionSeconds()} });
				lastPosition = now;
			}
			if (m_PlaybackService->ConsumeEndOfPlay())
			{
				PushEvent({ {"type", Protocol::Event::EndOfPlay} });
				HandleCommand({ CommandType::Stop });
			}

			timer.SleepFor(std::chrono::milliseconds(2));
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
		{
			Trace("[run] handling ", ToString(cmd.Type), " id=", cmd.RequestId);
			const CommandOutcome outcome = HandleCommand(cmd);
			Trace("[run] handled ", ToString(cmd.Type), " ok=", outcome.Ok);
			m_CommandsHandled.fetch_add(1, std::memory_order_acq_rel);

			if (!cmd.RequestId.empty())
				MessageQueue::GetInstance().PushTo(cmd.ClientId, ReplyJson(cmd.RequestId, outcome.Ok, outcome.Value, outcome.Error));
		}
	}

	CommandOutcome Application::HandleCommand(const Command& cmd)
	{
		const TransportState state = m_State.load(std::memory_order_acquire);

		if (!IsCommandLegal(state, cmd.Type))
		{
			const std::string reason = RejectionReason(state, cmd.Type);
			// A command with no id came from inside the engine or from a client that
			// wanted no acknowledgement, so the refusal has to travel as an event.
			if (cmd.RequestId.empty())
			{
				PushEvent({
					{"type", Protocol::Event::Error},
					{"value", reason},
					{"command", ToString(cmd.Type)},
					{"state", ToString(state)}
				});
			}
			return Failed(reason);
		}

		CommandOutcome outcome;

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
				if (cmd.RequestId.empty())
					PushError(error);
				// Everyone needs this one: it is what clears the loading message.
				PushEvent({ {"type", Protocol::Event::FileClosed} });
				outcome = Failed(error);
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
			PushEvent({ {"type", Protocol::Event::FileClosed} });
			break;
		case CommandType::Seek:
		{
			const double duration = m_Duration.load(std::memory_order_acquire);
			double seconds = (double)cmd.Value;
			seconds = std::clamp(seconds, 0.0, duration);
			m_PlaybackService->SeekToSample((uint64_t)(seconds * m_AudioData->SampleRate));
			break;
		}
		case CommandType::StepFrame:
		{
			const AnalysisParams& params = m_AnalysisService->GetParams();
			const uint64_t hop = (uint64_t)std::llround((double)params.HopSize / params.SampleRate * m_AudioData->SampleRate);
			const uint64_t currentSample = (uint64_t)std::llround(m_PlaybackService->GetPositionSeconds() * m_AudioData->SampleRate);
			if (currentSample + hop > (uint64_t)m_AudioData->SamplesPerChannel)
			{
				outcome = Failed("At the end of the track.");
				break;
			}
			const bool resetTrackers = m_AnalysisService->HasUnseenSeek();
			m_PlaybackService->SeekToSample(currentSample + hop);
			m_AnalysisService->RequestCurrentFrameAnalysis(resetTrackers);
			break;
		}
		case CommandType::ResetAnalysis:
			m_AnalysisService->ResetAnalysis();
			break;
		case CommandType::SetVolume:
			m_PlaybackService->SetVolume(std::clamp(cmd.Value, Protocol::VolumeMin, Protocol::VolumeMax));
			break;
		case CommandType::SetSpeed:
			m_PlaybackService->SetSpeed(std::clamp(cmd.Value, Protocol::SpeedMin, Protocol::SpeedMax));
			break;
		case CommandType::AnalyseFrame:
			m_AnalysisService->RequestCurrentFrameAnalysis();
			break;
		case CommandType::GetAnalysisSchema:
			outcome.Value = m_AnalysisService->GetSchemaJson();
			break;
		case CommandType::SetAnalysisSetting:
		{
			if (!cmd.Args.contains("stage") || !cmd.Args.at("stage").is_string()
				|| !cmd.Args.contains("key") || !cmd.Args.at("key").is_string()
				|| !cmd.Args.contains("value"))
			{
				outcome = Failed("setAnalysisSetting needs a stage, a key and a value.");
				break;
			}

			std::string error;
			if (!m_AnalysisService->SetSetting(cmd.Args.at("stage").get<std::string>(), cmd.Args.at("key").get<std::string>(), cmd.Args.at("value"), error))
				outcome = Failed(error);
			break;
		}
		case CommandType::Status:
			outcome.Value = GetStatusJson();
			break;
		case CommandType::Shutdown:
			PushInfo("Engine shutting down.");
			m_Running.store(false, std::memory_order_release);
			break;
		}

		if (ChangesTransport(cmd.Type))
			PushTransport();

		return outcome;
	}

	void Application::PushTransport()
	{
		PushEvent({ {"type", Protocol::Event::Transport}, {"value", GetStatusJson()} });
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
			for (int resolution : waveformBuilder.GetAvailableResolutions())
			{
				const auto& data = waveformBuilder.GetWaveformData(resolution);
				std::vector<float> peaks;
				peaks.reserve(data.size());
				for (const auto& peak : data)
					peaks.push_back(peak.Max);
				MessageQueue::GetInstance().PushBinary(BinaryFrame::EncodeWaveform(peaks, resolution));
			}
		});

		m_PcmFeeder->Init(m_AudioData);
		m_PcmFeeder->AddBuffer("Playback", 5.0f);

		if (m_PlaybackService->Init(m_PcmFeeder) < 0)
		{
			waveformThread.join();
			outError = "Could not open an audio output device.";
			return false;
		}
		m_AnalysisService->Init(m_PcmFeeder, AnalysisParams{ 8000, 4096, 64 }, m_PlaybackService.get());

		waveformThread.join();
		m_TrackPath = filePath;
		PushEvent({ {"type", Protocol::Event::FileLoaded}, {"value", { {"duration", m_AudioData->Duration} }} });
		return true;
	}

	void Application::ClearAudio()
	{
		try
		{
			m_AnalysisService->Reset();
			m_PlaybackService->Reset();
			m_PcmFeeder->Clear();
			m_AudioData->Clear();
			m_Duration.store(0.0, std::memory_order_release);
		}
		catch (...)
		{
			PushError("Failed to release the loaded file cleanly.");
		}
	}

	nlohmann::json Application::GetStatusJson() const
	{
		const TransportState state = m_State.load(std::memory_order_acquire);
		return {
			{"state", ToString(state)},
			{"track", HasFile(state)
				? nlohmann::json{ {"path", m_TrackPath}, {"sampleRate", m_AudioData->SampleRate}, {"channels", m_AudioData->Channels}, {"duration", m_Duration.load(std::memory_order_acquire)} }
				: nlohmann::json(nullptr)},
			{"position", m_PlaybackService->GetPositionSeconds()},
			{"speed", m_PlaybackService->GetSpeed()},
			{"volume", m_PlaybackService->GetVolume()},
			{"commandsHandled", m_CommandsHandled.load(std::memory_order_acquire)},
			{"underrunCount", m_PlaybackService->GetUnderrunCount()},
			{"maxCallbackUs", m_PlaybackService->GetMaxCallbackUs()},
			{"bytesSent", MessageQueue::GetInstance().GetBytesSent()}
		};
	}
}
