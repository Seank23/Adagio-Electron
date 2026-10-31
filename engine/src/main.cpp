#include "Core/EngineParams.h"
#include "Core/Application.h"
#include "Core/CommandParser.h"
#include "Core/CommandQueue.h"
#include "Core/MessageQueue.h"
#include "Core/Trace.h"
#include "Core/WebSocketServer.h"
#include "Protocol.generated.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <thread>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#endif

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

	// The saved preferences main passes at spawn.
	Adagio::EngineParams EngineParamsFromArgs(int argc, char** argv)
	{
		const std::pair<const char*, const char*> options[] = {
			{ "--sample-rate=", "sampleRate" },
			{ "--frame-length=", "frameLength" },
			{ "--hop-size=", "hopSize" },
			{ "--frame-smoothing=", "frameSmoothing" }
		};

		Adagio::EngineParams params;
		for (const auto& [prefix, key] : options)
		{
			const std::string text = ValueFromArgs(argc, argv, prefix);
			if (text.empty())
				continue;

			const nlohmann::json value = nlohmann::json::parse(text, nullptr, false);
			std::string error;
			if (!Adagio::UpdateEngineParams(params, { { key, value } }, error))
				std::cerr << "Ignoring " << prefix << text << ": " << error << "\n" << std::flush;
		}
		return params;
	}

	// main passes userData's pipelines.json. Without it, the file beside the executable, so a dev engine started by hand
	// keeps its settings apart from the installed app's.
	std::filesystem::path PipelinesFileFromArgs(int argc, char** argv)
	{
#ifdef _WIN32
		const std::wstring prefix = L"--pipelines=";
		std::filesystem::path file;
		int wideArgc = 0;
		if (LPWSTR* wideArgv = CommandLineToArgvW(GetCommandLineW(), &wideArgc))
		{
			for (int i = 1; i < wideArgc && file.empty(); ++i)
			{
				const std::wstring arg = wideArgv[i];
				if (arg.rfind(prefix, 0) == 0)
					file = arg.substr(prefix.size());
			}
			LocalFree(wideArgv);
		}
		if (!file.empty())
			return file;

		std::wstring executable(MAX_PATH, L'\0');
		DWORD length = 0;
		while ((length = GetModuleFileNameW(nullptr, executable.data(), (DWORD)executable.size())) == executable.size())
			executable.resize(executable.size() * 2);
		executable.resize(length);
		return std::filesystem::path(executable).parent_path() / L"pipelines.json";
#else
		const std::string file = ValueFromArgs(argc, argv, "--pipelines=");
		return file.empty() ? std::filesystem::path("pipelines.json") : std::filesystem::path(file);
#endif
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

	Adagio::Application app(EngineParamsFromArgs(argc, argv), PipelinesFileFromArgs(argc, argv));
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
