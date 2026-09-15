#pragma once
#include <string>

namespace Adagio
{
	enum class CommandType
	{
		Play,
		Pause,
		Stop,
		Load,
		Clear,
		Seek,
		SetVolume,
		SetSpeed,
		AnalyseFrame,
		Status,
		Shutdown
	};

	const char* ToString(CommandType type);

	struct Command
	{
		CommandType Type = CommandType::Status;
		float Value = 0.0f;
		std::string StrValue;
	};
}
