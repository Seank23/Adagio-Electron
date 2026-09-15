#include "Command.h"

namespace Adagio
{
	const char* ToString(CommandType type)
	{
		switch (type)
		{
		case CommandType::Play:         return "play";
		case CommandType::Pause:        return "pause";
		case CommandType::Stop:         return "stop";
		case CommandType::Load:         return "load";
		case CommandType::Clear:        return "clear";
		case CommandType::Seek:         return "seek";
		case CommandType::SetVolume:    return "setVolume";
		case CommandType::SetSpeed:     return "setSpeed";
		case CommandType::AnalyseFrame: return "analyseFrame";
		case CommandType::Status:       return "status";
		case CommandType::Shutdown:     return "shutdown";
		}
		return "unknown";
	}
}
