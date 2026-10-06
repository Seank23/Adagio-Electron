#include "Analysis/AnalysisParams.h"
#include "Core/Application.h"
#include "Core/CommandParser.h"
#include "Core/CommandQueue.h"
#include "Core/MessageQueue.h"
#include "Core/Trace.h"
#include "Core/WebSocketServer.h"
#include "Protocol.generated.h"

#include <iostream>
#include <string>
#include <thread>

namespace
{
	std::string ValueFromArgs(int argc, char** argv, const std::string& prefix)
	{
		for (int i = 1; i < argc; ++i)
		{
			const std::string arg = argv[i];
			if (arg.rfind(prefix, 0) == 0)
				return arg.substr(prefix.size());
		}
		return {};
	}

	// The saved preferences main passes at spawn. A bad value is a warning, never a reason not to start:
	// that one keeps its default and the others still apply.
	Adagio::AnalysisParams AnalysisParamsFromArgs(int argc, char** argv)
	{
		const std::pair<const char*, const char*> options[] = {
			{ "--sample-rate=", "sampleRate" },
			{ "--frame-length=", "frameLength" },
			{ "--hop-size=", "hopSize" }
		};

		Adagio::AnalysisParams params;
		for (const auto& [prefix, key] : options)
		{
			const std::string text = ValueFromArgs(argc, argv, prefix);
			if (text.empty())
				continue;

			const nlohmann::json value = nlohmann::json::parse(text, nullptr, false);
			std::string error;
			if (!Adagio::UpdateAnalysisParams(params, { { key, value } }, error))
				std::cerr << "Ignoring " << prefix << text << ": " << error << "\n" << std::flush;
		}
		return params;
	}

	bool HasFlag(int argc, char** argv, const std::string& flag)
	{
		for (int i = 1; i < argc; ++i)
		{
			if (flag == argv[i])
				return true;
		}
		return false;
	}

	// Runs on the connection's own thread, so it does no more than read the frame:
	// the work goes onto the queue that the command thread drains.
	void OnClientMessage(const std::string& clientId, const std::string& message)
	{
		Adagio::ParsedCommand parsed = Adagio::ParseCommand(message, clientId);
		if (parsed.Ok)
		{
			Adagio::Trace("[cmd] queued ", Adagio::ToString(parsed.Value.Type), " id=", parsed.RequestId);
			Adagio::CommandQueue::GetInstance().Push(parsed.Value);
			return;
		}

		Adagio::Trace("[cmd] refused: ", parsed.Error);

		if (!parsed.RequestId.empty())
		{
			Adagio::MessageQueue::GetInstance().PushTo(clientId, Adagio::ReplyJson(parsed.RequestId, false, {}, parsed.Error));
			return;
		}

		const nlohmann::json error = { {"type", Adagio::Protocol::Event::Error}, {"value", parsed.Error} };
		Adagio::MessageQueue::GetInstance().PushTo(clientId, error.dump());
	}
}

int main(int argc, char** argv)
{
	Adagio::SetTracing(HasFlag(argc, argv, "--trace"));

	Adagio::Application app(AnalysisParamsFromArgs(argc, argv));
	Adagio::WSServer wsServer(Adagio::Protocol::Port, ValueFromArgs(argc, argv, "--token="));
	wsServer.SetCommandHandler(OnClientMessage);

	if (!wsServer.Start())
	{
		std::cerr << "Adagio engine could not listen on port " << Adagio::Protocol::Port << ".\n" << std::flush;
		return 1;
	}

	// Electron closes our stdin when it exits, so EOF here means the app is gone and
	// the engine should not outlive it holding port 9001.
	std::thread stdinThread([&]()
		{
			std::string line;
			while (std::getline(std::cin, line))
			{
			}
			Adagio::CommandQueue::GetInstance().Push({ Adagio::CommandType::Shutdown });
		});

	std::cout << "Adagio engine ready\n" << std::flush;

	app.Run();

	wsServer.Stop();
	stdinThread.detach();
	return 0;
}
