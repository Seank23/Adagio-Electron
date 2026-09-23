#include "CommandParser.h"

namespace Adagio
{
	namespace
	{
		ParsedCommand Refuse(const std::string& requestId, const std::string& error)
		{
			ParsedCommand parsed;
			parsed.Ok = false;
			parsed.Error = error;
			parsed.RequestId = requestId;
			return parsed;
		}

		std::string ReadRequestId(const nlohmann::json& message)
		{
			if (!message.contains("id"))
				return {};

			const auto& id = message.at("id");
			if (id.is_string())
				return id.get<std::string>();
			if (id.is_number_integer())
				return std::to_string(id.get<int64_t>());
			return {};
		}
	}

	ParsedCommand ParseCommand(const std::string& message, const std::string& clientId)
	{
		const nlohmann::json parsedJson = nlohmann::json::parse(message, nullptr, false);
		if (parsedJson.is_discarded() || !parsedJson.is_object())
			return Refuse({}, "Message is not a JSON object.");

		const std::string requestId = ReadRequestId(parsedJson);

		if (!parsedJson.contains("cmd") || !parsedJson.at("cmd").is_string())
			return Refuse(requestId, "Message has no cmd field.");

		const std::string name = parsedJson.at("cmd").get<std::string>();
		CommandType type = CommandType::Status;
		if (!CommandFromString(name, type))
			return Refuse(requestId, "Unknown command '" + name + "'.");

		const nlohmann::json args = parsedJson.contains("args") ? parsedJson.at("args") : nlohmann::json();

		ParsedCommand parsed;
		parsed.RequestId = requestId;
		parsed.Value.Type = type;
		parsed.Value.RequestId = requestId;
		parsed.Value.ClientId = clientId;

		switch (ArgumentKind(type))
		{
		case CommandArg::Number:
			if (!args.is_number())
				return Refuse(requestId, "Command '" + name + "' needs a number in args.");
			parsed.Value.Value = args.get<float>();
			break;
		case CommandArg::String:
			if (!args.is_string())
				return Refuse(requestId, "Command '" + name + "' needs a string in args.");
			parsed.Value.StrValue = args.get<std::string>();
			break;
		case CommandArg::Object:
			if (!args.is_object())
				return Refuse(requestId, "Command '" + name + "' needs an object in args.");
			parsed.Value.Args = args;
			break;
		case CommandArg::None:
			break;
		}

		parsed.Ok = true;
		return parsed;
	}

	std::string ReplyJson(const std::string& requestId, bool ok, const nlohmann::json& value, const std::string& error)
	{
		nlohmann::json reply = {
			{"type", Protocol::ReplyType},
			{"id", requestId},
			{"ok", ok}
		};
		if (!value.is_null())
			reply["value"] = value;
		if (!ok)
			reply["error"] = error;
		return reply.dump();
	}
}
