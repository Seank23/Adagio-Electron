#pragma once
#include <ixwebsocket/IXWebSocketServer.h>
#include <ixwebsocket/IXNetSystem.h>
#include <atomic>
#include <functional>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

namespace Adagio
{
	class WSServer
	{
	public:
		// Bounds how long a connection's poll can wait. See the constructor.
		static constexpr int PingIntervalSeconds = 1;

		// Called on the connection's own thread for every accepted frame. The handler
		// parses it and queues a command; it must not block.
		using CommandHandler = std::function<void(const std::string& clientId, const std::string& message)>;

		WSServer(int port, std::string token = {});

		void SetCommandHandler(CommandHandler handler) { m_CommandHandler = std::move(handler); }

		// False when the port could not be bound
		bool Start();
		void Stop();

		void SendToClient(const std::string& clientId, const std::string& message, bool binary = false);
		void Broadcast(const std::string& message, bool binary = false);

	private:
		void ProcessQueue();
		static void Send(ix::WebSocket& socket, const std::string& message, bool binary);
		void OnOpen(const std::string& clientId, ix::WebSocket& webSocket, const ix::WebSocketOpenInfo& openInfo);
		std::string RefusalReason(const ix::WebSocketOpenInfo& openInfo) const;

		std::unique_ptr<ix::WebSocketServer> m_Server;
		std::mutex m_Mutex;

		std::atomic<bool> m_Running;
		std::thread m_QueueThread;
		std::string m_Token;
		CommandHandler m_CommandHandler;

		std::unordered_map<std::string, std::shared_ptr<ix::WebSocket>> m_Clients;
	};
}
