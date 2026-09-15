#pragma once
#include "TransportState.h"

#include <atomic>
#include <memory>
#include <string>

namespace Adagio
{
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

		// Read-only snapshot, safe to call from the HTTP thread while Run() is
		// draining commands. Everything it touches is an atomic.
		std::string GetStatusJson() const;
		TransportState GetTransportState() const { return m_State.load(std::memory_order_acquire); }

	private:
		void ProcessCommands();
		void HandleCommand(const Command& cmd);
		void Shutdown();

		bool LoadAudio(const std::string& filePath, std::string& outError);
		void ClearAudio();

		std::shared_ptr<AudioDecoder> m_AudioDecoder;
		std::unique_ptr<FileIOService> m_FileIOService;
		std::unique_ptr<PlaybackService> m_PlaybackService;
		std::unique_ptr<AnalysisService> m_AnalysisService;

		std::shared_ptr<AudioData> m_AudioData;

		std::atomic<TransportState> m_State{ TransportState::Empty };
		std::atomic<bool> m_Running{ false };
		std::atomic<double> m_Duration{ 0.0 };
		// Published so a caller can tell "the command ran and was refused" from
		// "the command has not been dequeued yet" without guessing at timings.
		std::atomic<uint64_t> m_CommandsHandled{ 0 };
	};
}
