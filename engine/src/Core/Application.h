#pragma once
#include "TransportState.h"

#include <nlohmann/json.hpp>

#include <atomic>
#include <memory>
#include <string>

namespace Adagio
{
	class AudioData;
	class PcmFeeder;
	class FileIOService;
	class PlaybackService;
	class AnalysisService;

	struct CommandOutcome
	{
		bool Ok = true;
		std::string Error;
		nlohmann::json Value;
	};

	class Application
	{
	public:
		Application();
		virtual ~Application();

		void Run();

		nlohmann::json GetStatusJson() const;
		TransportState GetTransportState() const { return m_State.load(std::memory_order_acquire); }

	private:
		void ProcessCommands();
		CommandOutcome HandleCommand(const Command& cmd);
		void PushTransport();
		void Shutdown();

		bool LoadAudio(const std::string& filePath, std::string& outError);
		void ClearAudio();

		std::shared_ptr<PcmFeeder> m_PcmFeeder;
		std::unique_ptr<FileIOService> m_FileIOService;
		std::unique_ptr<PlaybackService> m_PlaybackService;
		std::unique_ptr<AnalysisService> m_AnalysisService;

		std::shared_ptr<AudioData> m_AudioData;

		std::atomic<TransportState> m_State{ TransportState::Empty };
		std::atomic<bool> m_Running{ false };
		std::atomic<double> m_Duration{ 0.0 };

		std::atomic<uint64_t> m_CommandsHandled{ 0 };
	};
}
