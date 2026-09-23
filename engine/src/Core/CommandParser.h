#pragma once
#include "Command.h"

#include <string>

namespace Adagio
{
	// The result of reading one inbound frame. A malformed frame still carries the id
	// it arrived with, so the sender gets an addressed refusal rather than silence.
	struct ParsedCommand
	{
		bool Ok = false;
		std::string Error;
		std::string RequestId;
		Command Value;
	};

	// Reads {id, cmd, args} and checks the argument against the kind protocol.json
	// declares for that command. Never throws: a bad frame comes back as an error.
	ParsedCommand ParseCommand(const std::string& message, const std::string& clientId);

	std::string ReplyJson(const std::string& requestId, bool ok, const nlohmann::json& value, const std::string& error);
}
