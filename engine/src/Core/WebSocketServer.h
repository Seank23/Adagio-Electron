#pragma once
#include <ixwebsocket/IXWebSocketServer.h>
#include <ixwebsocket/IXNetSystem.h>
#include <atomic>
#include <iostream>
#include <mutex>
#include <thread>
#include <unordered_map>

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

		// Read by the queue thread and written by Stop(), so it has to be atomic.
		std::atomic<bool> m_Running;
		std::thread m_QueueThread;
		std::unordered_map<std::string, std::shared_ptr<ix::WebSocket>> m_Clients;
	};
}
