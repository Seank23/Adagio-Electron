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
	std::string TokenFromArgs(int argc, char** argv)
	{
		const std::string prefix = "--token=";
		for (int i = 1; i < argc; ++i)
		{
			const std::string arg = argv[i];
			if (arg.rfind(prefix, 0) == 0)
				return arg.substr(prefix.size());
		}
		return {};
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

	Adagio::Application app;
	Adagio::WSServer wsServer(Adagio::Protocol::Port, TokenFromArgs(argc, argv));
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
