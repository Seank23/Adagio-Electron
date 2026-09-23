#include "Command.h"

namespace Adagio
{
	const char* ToString(CommandType type)
	{
		switch (type)
		{
#define ADAGIO_COMMAND_NAME(cppName, wireName, argKind) case CommandType::cppName: return wireName;
			ADAGIO_PROTOCOL_COMMANDS(ADAGIO_COMMAND_NAME)
#undef ADAGIO_COMMAND_NAME
		}
		return "unknown";
	}

	CommandArg ArgumentKind(CommandType type)
	{
		switch (type)
		{
#define ADAGIO_COMMAND_ARG(cppName, wireName, argKind) case CommandType::cppName: return CommandArg::argKind;
			ADAGIO_PROTOCOL_COMMANDS(ADAGIO_COMMAND_ARG)
#undef ADAGIO_COMMAND_ARG
		}
		return CommandArg::None;
	}

	bool CommandFromString(const std::string& name, CommandType& outType)
	{
#define ADAGIO_COMMAND_LOOKUP(cppName, wireName, argKind) if (name == wireName) { outType = CommandType::cppName; return true; }
		ADAGIO_PROTOCOL_COMMANDS(ADAGIO_COMMAND_LOOKUP)
#undef ADAGIO_COMMAND_LOOKUP
		return false;
	}
}
