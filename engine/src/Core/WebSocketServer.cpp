#include "WebSocketServer.h"
#include "MessageQueue.h"

namespace Adagio
{
	WSServer::WSServer(int port)
		: m_Running(false)
	{
        m_Server = std::make_unique<ix::WebSocketServer>(9001);
	}

	void WSServer::Start()
	{
        // Required on Windows; safe everywhere.
        ix::initNetSystem();
        m_Running = true;

        // Register the connection handler
        m_Server->setOnConnectionCallback(
            [this](std::weak_ptr<ix::WebSocket> webSocket, std::shared_ptr<ix::ConnectionState> connectionState)
            {
                std::string clientId = connectionState->getId();

                std::cout << "Client connected: " << clientId << std::endl;

                // Handle incoming messages
                webSocket.lock()->setOnMessageCallback(
                    [this, clientId](const ix::WebSocketMessagePtr& msg)
                    {
                        if (msg->type == ix::WebSocketMessageType::Message)
                        {
                            std::cout << "Client " << clientId
                                << " says: " << msg->str << "\n";

                            // Echo or send reply
                            SendToClient(clientId, "Hello from C++ engine!");
                        }
                        else if (msg->type == ix::WebSocketMessageType::Close)
                        {
                            std::cout << "Client closed: " << clientId << "\n";
                        }
                        else if (msg->type == ix::WebSocketMessageType::Error)
                        {
                            std::cout << "Error: " << msg->errorInfo.reason << "\n";
                        }
                    });

                // Store the socket so we can message it later
                std::lock_guard<std::mutex> lock(m_Mutex);
                m_Clients[clientId] = webSocket.lock();
            });

        // Start server
        auto res = m_Server->listen();
        if (!res.first)
        {
            std::cerr << "Failed to listen on port: " << m_Server->getPort() << "\n";
            return;
        }

        m_Server->start();
        std::cout << "WebSocket server running on port " << m_Server->getPort() << "\n";

        std::thread([&]()
            {
                while (m_Running)
                {
                    ProcessQueue();
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                }
            }).detach();
	}

    void WSServer::Stop()
    {
        m_Server->stop();
        ix::uninitNetSystem();
        m_Running = false;
    }

    void WSServer::SendToClient(const std::string& clientId, const std::string& message)
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        if (auto it = m_Clients.find(clientId); it != m_Clients.end())
            it->second->send(message);
    }

    void WSServer::Broadcast(const std::string& message)
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        for (auto& [id, socket] : m_Clients)
            socket->send(message);
    }

    void WSServer::ProcessQueue()
    {
        std::string msg;

        while (MessageQueue::Instance().Pop(msg))
            Broadcast(msg);
    }
}