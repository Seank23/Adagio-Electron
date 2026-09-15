#include "TransportState.h"

namespace Adagio
{
	const char* ToString(TransportState state)
	{
		switch (state)
		{
		case TransportState::Empty:   return "empty";
		case TransportState::Loading: return "loading";
		case TransportState::Ready:   return "ready";
		case TransportState::Playing: return "playing";
		case TransportState::Paused:  return "paused";
		}
		return "unknown";
	}

	namespace
	{
		bool HasFile(TransportState state)
		{
			return state == TransportState::Ready
				|| state == TransportState::Playing
				|| state == TransportState::Paused;
		}
	}

	bool IsCommandLegal(TransportState state, CommandType command)
	{
		switch (command)
		{
		// Always available: they either cost nothing or are how the host talks to us.
		case CommandType::SetVolume:
		case CommandType::Shutdown:
		case CommandType::Status:
			return true;

		case CommandType::Load:
			return state != TransportState::Loading;

		case CommandType::Pause:
			return state == TransportState::Playing || state == TransportState::Paused;

		case CommandType::Play:
		case CommandType::Stop:
		case CommandType::Clear:
		case CommandType::Seek:
		case CommandType::SetSpeed:
		case CommandType::AnalyseFrame:
			return HasFile(state);
		}
		return false;
	}

	TransportState NextState(TransportState state, CommandType command)
	{
		switch (command)
		{
		case CommandType::Load:
			return TransportState::Loading;
		case CommandType::Play:
			return TransportState::Playing;
		case CommandType::Pause:
			return TransportState::Paused;
		case CommandType::Stop:
			return TransportState::Ready;
		case CommandType::Clear:
			return TransportState::Empty;

		// Seek, speed, volume, analysis, status and shutdown never move the transport.
		default:
			return state;
		}
	}

	const char* RejectionReason(TransportState state, CommandType command)
	{
		if (command == CommandType::Load && state == TransportState::Loading)
			return "A file is already loading.";
		if (state == TransportState::Loading)
			return "The engine is loading a file.";
		if (state == TransportState::Empty)
			return "No audio file is loaded.";
		if (command == CommandType::Pause)
			return "Nothing is playing.";
		return "The command is not valid in this state.";
	}
}
