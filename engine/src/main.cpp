#include <iostream>
#include <thread>
#include <atomic>
#include "httplib.h"
#include "Core/Application.h"
#include "API/Utils.h"
#include "Core/WebSocketServer.h"
#include "Core/CommandQueue.h"

int main(int argc, char** argv) 
{
	Adagio::Application app;

    httplib::Server svr;
    Adagio::WSServer wsServer(9001);

    svr.Post("/load", [&](const httplib::Request& req, httplib::Response& res)
        {
            std::string path = req.body;
            Adagio::CommandQueue::Instance().Push({ CommandType::Load, 0.0f, path });
			res.set_content("{ \"status\": \"success\" }", "application/json");
        });

    svr.Post("/play", [&](const httplib::Request& req, httplib::Response& res)
        {
            Adagio::CommandQueue::Instance().Push({ CommandType::Play });
			res.set_content("{ \"status\": \"success\" }", "application/json");
		});

    svr.Post("/pause", [&](const httplib::Request& req, httplib::Response& res)
        {
            Adagio::CommandQueue::Instance().Push({ CommandType::Pause });
            res.set_content("{ \"status\": \"success\" }", "application/json");
        });

    svr.Post("/stop", [&](const httplib::Request& req, httplib::Response& res)
        {
            Adagio::CommandQueue::Instance().Push({ CommandType::Stop });
            res.set_content("{ \"status\": \"success\" }", "application/json");
        });

    svr.Post("/clear", [&](const httplib::Request& req, httplib::Response& res)
        {
            Adagio::CommandQueue::Instance().Push({ CommandType::Clear });
            res.set_content("{ \"status\": \"success\" }", "application/json");
        });

    svr.Post("/volume", [&](const httplib::Request& req, httplib::Response& res)
        {
            float volume = std::stof(req.body) / 100.0f;
            Adagio::CommandQueue::Instance().Push({ CommandType::SetVolume, volume });
            res.set_content("{ \"status\": \"success\" }", "application/json");
        });

    svr.Post("/seek", [&](const httplib::Request& req, httplib::Response& res)
        {
            float seconds = std::stof(req.body);
            Adagio::CommandQueue::Instance().Push({ CommandType::Seek, seconds });
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