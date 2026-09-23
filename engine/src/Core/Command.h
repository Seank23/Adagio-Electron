#pragma once
#include "Protocol.generated.h"

#include <nlohmann/json.hpp>

#include <string>

namespace Adagio
{
	enum class CommandArg
	{
		None,
		Number,
		String,
		Object
	};

	enum class CommandType
	{
#define ADAGIO_COMMAND_ENUM(cppName, wireName, argKind) cppName,
		ADAGIO_PROTOCOL_COMMANDS(ADAGIO_COMMAND_ENUM)
#undef ADAGIO_COMMAND_ENUM
	};

	const char* ToString(CommandType type);
	CommandArg ArgumentKind(CommandType type);
	bool CommandFromString(const std::string& name, CommandType& outType);

	struct Command
	{
		CommandType Type = CommandType::Status;
		float Value = 0.0f;
		std::string StrValue;
		// Only the object commands read this. Number and string arguments are unpacked
		// into Value and StrValue by the parser.
		nlohmann::json Args;
		// Empty when nobody is waiting: an internally queued command, or a client that
		// sent no id and wants no acknowledgement.
		std::string RequestId;
		std::string ClientId;
	};
}
