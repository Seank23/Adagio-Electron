#include "Core/Application.h"
#include "Core/CommandQueue.h"
#include "Core/WebSocketServer.h"

#include "httplib.h"

#include <atomic>
#include <iostream>
#include <string>
#include <thread>

namespace
{
	void Push(Adagio::CommandType type, float value = 0.0f, std::string text = {})
	{
		Adagio::CommandQueue::GetInstance().Push({ type, value, std::move(text) });
	}

	void Accepted(httplib::Response& res)
	{
		res.set_content("{ \"status\": \"accepted\" }", "application/json");
	}
}

int main(int argc, char** argv)
{
	Adagio::Application app;

	httplib::Server svr;
	Adagio::WSServer wsServer(9001);

	svr.Post("/load", [&](const httplib::Request& req, httplib::Response& res)
		{
			Push(Adagio::CommandType::Load, 0.0f, req.body);
			Accepted(res);
		});

	svr.Post("/play", [&](const httplib::Request& req, httplib::Response& res)
		{
			Push(Adagio::CommandType::Play);
			Accepted(res);
		});

	svr.Post("/pause", [&](const httplib::Request& req, httplib::Response& res)
		{
			Push(Adagio::CommandType::Pause);
			Accepted(res);
		});

	svr.Post("/stop", [&](const httplib::Request& req, httplib::Response& res)
		{
			Push(Adagio::CommandType::Stop);
			Accepted(res);
		});

	svr.Post("/clear", [&](const httplib::Request& req, httplib::Response& res)
		{
			Push(Adagio::CommandType::Clear);
			Accepted(res);
		});

	svr.Post("/volume", [&](const httplib::Request& req, httplib::Response& res)
		{
			try
			{
				Push(Adagio::CommandType::SetVolume, std::stof(req.body) / 100.0f);
				Accepted(res);
			}
			catch (const std::exception&)
			{
				res.status = 400;
				res.set_content("{ \"status\": \"error\", \"value\": \"volume must be a number\" }", "application/json");
			}
		});

	svr.Post("/seek", [&](const httplib::Request& req, httplib::Response& res)
		{
			try
			{
				Push(Adagio::CommandType::Seek, std::stof(req.body));
				Accepted(res);
			}
			catch (const std::exception&)
			{
				res.status = 400;
				res.set_content("{ \"status\": \"error\", \"value\": \"seek must be a number\" }", "application/json");
			}
		});

	svr.Post("/requestAnalysis", [&](const httplib::Request& req, httplib::Response& res)
		{
			Push(Adagio::CommandType::AnalyseFrame);
			Accepted(res);
		});

	svr.Post("/speed", [&](const httplib::Request& req, httplib::Response& res)
		{
			try
			{
				Push(Adagio::CommandType::SetSpeed, std::stof(req.body) / 100.0f);
				Accepted(res);
			}
			catch (const std::exception&)
			{
				res.status = 400;
				res.set_content("{ \"status\": \"error\", \"value\": \"speed must be a number\" }", "application/json");
			}
		});

	// The one route that answers rather than queues. It is a read-only snapshot of
	// atomics, so it doubles as the liveness probe: if this returns, the command
	// thread survived whatever was sent before it.
	svr.Get("/status", [&](const httplib::Request& req, httplib::Response& res)
		{
			res.set_content(app.GetStatusJson(), "application/json");
		});

	svr.Post("/shutdown", [&](const httplib::Request& req, httplib::Response& res)
		{
			Push(Adagio::CommandType::Shutdown);
			Accepted(res);
		});

	std::thread wsThread([&]() { wsServer.Start(); });
	std::thread serverThread([&]()
		{
			std::cout << "Starting server on http://127.0.0.1:5000\n";
			svr.listen("127.0.0.1", 5000);
		});

	// Electron closes our stdin when it exits, so EOF here means the app is gone and
	// the engine should not outlive it holding ports 5000 and 9001.
	std::thread stdinThread([&]()
		{
			std::string line;
			while (std::getline(std::cin, line))
			{
			}
			Push(Adagio::CommandType::Shutdown);
		});

	std::cout << "Adagio engine ready\n" << std::flush;

	app.Run();

	svr.stop();
	wsServer.Stop();
	serverThread.join();
	wsThread.join();
	stdinThread.detach();
	return 0;
}
