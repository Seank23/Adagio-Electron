#include <iostream>
#include <thread>
#include <atomic>
#include "httplib.h"
#include "Core/Application.h"

int main(int argc, char** argv) 
{
	Adagio::Application app;

    std::atomic<bool> isRunning = false;

    httplib::Server svr;

    svr.Post("/load", [&](const httplib::Request& req, httplib::Response& res)
        {
            try
            {
                std::string path = req.body;
				int loadSuccess = app.LoadAudio(path);
                if (loadSuccess == 0)
                {
                    res.status = 500;
                    res.set_content("{\"status\":\"failed to open file\"}", "application/json");
                    return;
                }
                res.set_content("{ \"status\": \"Audio loaded successfully.\" }", "application/json");
            }
            catch (const std::exception& e)
            {
                res.status = 500;
				res.set_content(std::string("{\"status\":\"") + e.what() + "\"}", "application/json");
            }
        });

    svr.Post("/play", [&](const httplib::Request& req, httplib::Response& res)
        {
            app.UpdateAudioState(Adagio::PlayState::PLAYING);
            res.set_content("{ \"status\": \"Playback started.\" }", "application/json");
		});

    svr.Post("/pause", [&](const httplib::Request& req, httplib::Response& res)
        {
            app.UpdateAudioState(Adagio::PlayState::PAUSED);
            res.set_content("{ \"status\": \"Playback paused.\" }", "application/json");
        });

    std::thread serverThread([&]()
        {
            std::cout << "Starting server on http://127.0.0.1:5000\n";
            svr.listen("127.0.0.1", 5000);
        });

    //std::signal(SIGINT, [](int){});

	serverThread.join();
    return 0;
}