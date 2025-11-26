#pragma once
#include <ixwebsocket/IXWebSocketServer.h>
#include <ixwebsocket/IXNetSystem.h>
#include <iostream>
#include <thread>

namespace Adagio
{
	class WSServer
	{
	public:
		WSServer(int port);

		void Start();
		void Stop();

		void SendToClient(const std::string& clientId, const std::string& message);
		void Broadcast(const std::string& message);

	private:
		void ProcessQueue();

		std::unique_ptr<ix::WebSocketServer> m_Server;
		std::mutex m_Mutex;

		bool m_Running;
		std::unordered_map<std::string, std::shared_ptr<ix::WebSocket>> m_Clients;
	};
}