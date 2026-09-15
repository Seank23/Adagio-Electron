#include "Core/Application.h"
#include "Core/CommandQueue.h"
#include "Core/WebSocketServer.h"
#include "API/Utils.h"

#include "httplib.h"

#include <atomic>
#include <iostream>
#include <thread>

int main(int argc, char** argv)
{
	Adagio::Application app;

	httplib::Server svr;
	Adagio::WSServer wsServer(9001);

	svr.Post("/load", [&](const httplib::Request& req, httplib::Response& res)
		{
			std::string path = req.body;
			Adagio::CommandQueue::GetInstance().Push({ Adagio::CommandType::Load, 0.0f, path });
			res.set_content("{ \"status\": \"success\" }", "application/json");
		});

	svr.Post("/play", [&](const httplib::Request& req, httplib::Response& res)
		{
			Adagio::CommandQueue::GetInstance().Push({ Adagio::CommandType::Play });
			res.set_content("{ \"status\": \"success\" }", "application/json");
		});

	svr.Post("/pause", [&](const httplib::Request& req, httplib::Response& res)
		{
			Adagio::CommandQueue::GetInstance().Push({ Adagio::CommandType::Pause });
			res.set_content("{ \"status\": \"success\" }", "application/json");
		});

	svr.Post("/stop", [&](const httplib::Request& req, httplib::Response& res)
		{
			Adagio::CommandQueue::GetInstance().Push({ Adagio::CommandType::Stop });
			res.set_content("{ \"status\": \"success\" }", "application/json");
		});

	svr.Post("/clear", [&](const httplib::Request& req, httplib::Response& res)
		{
			Adagio::CommandQueue::GetInstance().Push({ Adagio::CommandType::Clear });
			res.set_content("{ \"status\": \"success\" }", "application/json");
		});

	svr.Post("/volume", [&](const httplib::Request& req, httplib::Response& res)
		{
			float volume = std::stof(req.body) / 100.0f;
			Adagio::CommandQueue::GetInstance().Push({ Adagio::CommandType::SetVolume, volume });
			res.set_content("{ \"status\": \"success\" }", "application/json");
		});

	svr.Post("/seek", [&](const httplib::Request& req, httplib::Response& res)
		{
			float seconds = std::stof(req.body);
			Adagio::CommandQueue::GetInstance().Push({ Adagio::CommandType::Seek, seconds });
			res.set_content("{ \"status\": \"success\" }", "application/json");
		});

	svr.Post("/requestAnalysis", [&](const httplib::Request& req, httplib::Response& res)
		{
			float seconds = std::stof(req.body);
			Adagio::CommandQueue::GetInstance().Push({ Adagio::CommandType::AnalyseFrame });
			res.set_content("{ \"status\": \"success\" }", "application/json");
		});

	svr.Post("/speed", [&](const httplib::Request& req, httplib::Response& res)
		{
			float speed = std::stof(req.body) / 100.0f;
			Adagio::CommandQueue::GetInstance().Push({ Adagio::CommandType::SetSpeed, speed });
			res.set_content("{ \"status\": \"success\" }", "application/json");
		});

	std::thread appThread([&]()
		{
			app.Run();
		});

	std::thread serverThread([&]()
		{
			std::cout << "Starting server on http://127.0.0.1:5000\n";
			svr.listen("127.0.0.1", 5000);
		});

	std::thread wsThread([&]()
		{
			wsServer.Start();
		});
	wsThread.detach();

	serverThread.join();
	appThread.join();
}
