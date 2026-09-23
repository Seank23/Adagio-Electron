#include "WebSocketServer.h"
#include "MessageQueue.h"
#include "Protocol.generated.h"
#include "Trace.h"

#include <algorithm>
#include <vector>

namespace Adagio
{
	namespace
	{
		// The handshake target is "/?token=..."
		std::string QueryValue(const std::string& uri, const std::string& key)
		{
			const size_t queryStart = uri.find('?');
			if (queryStart == std::string::npos)
				return {};

			const std::string needle = key + "=";
			size_t pos = queryStart + 1;
			while (pos < uri.size())
			{
				const size_t end = std::min(uri.find('&', pos), uri.size());
				if (uri.compare(pos, needle.size(), needle) == 0)
					return uri.substr(pos + needle.size(), end - pos - needle.size());
				pos = end + 1;
			}
			return {};
		}

		bool ConstantTimeEquals(const std::string& a, const std::string& b)
		{
			if (a.size() != b.size())
				return false;

			unsigned char difference = 0;
			for (size_t i = 0; i < a.size(); ++i)
				difference |= static_cast<unsigned char>(a[i] ^ b[i]);
			return difference == 0;
		}
	}

	WSServer::WSServer(int port, std::string token)
		: m_Running(false), m_Token(std::move(token))
	{
		m_Server = std::make_unique<ix::WebSocketServer>(
			port,
			Protocol::Host,
			ix::SocketServer::kDefaultTcpBacklog,
			ix::SocketServer::kDefaultMaxConnections,
			ix::WebSocketServer::kDefaultHandShakeTimeoutSecs,
			ix::SocketServer::kDefaultAddressFamily,
			PingIntervalSeconds);
	}

	bool WSServer::Start()
	{
		// Required on Windows; safe everywhere.
		ix::initNetSystem();
		m_Running.store(true, std::memory_order_release);

		m_Server->setOnClientMessageCallback(
			[this](std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, const ix::WebSocketMessagePtr& msg)
			{
				const std::string clientId = connectionState->getId();

				switch (msg->type)
				{
				case ix::WebSocketMessageType::Open:
					OnOpen(clientId, webSocket, msg->openInfo);
					break;
				case ix::WebSocketMessageType::Message:
				{
					Trace("[ws] rx ", msg->str.size(), " bytes from ", clientId);
					std::lock_guard<std::mutex> lock(m_Mutex);
					if (m_Clients.find(clientId) == m_Clients.end())
					{
						Trace("[ws] dropped: ", clientId, " is not a registered client");
						return;
					}
					if (m_CommandHandler)
						m_CommandHandler(clientId, msg->str);
					break;
				}
				case ix::WebSocketMessageType::Close:
				{
					Trace("[ws] close ", clientId, " code=", msg->closeInfo.code, " reason=", msg->closeInfo.reason);
					std::lock_guard<std::mutex> lock(m_Mutex);
					m_Clients.erase(clientId);
					break;
				}
				case ix::WebSocketMessageType::Error:
					std::cerr << "WebSocket error: " << msg->errorInfo.reason << "\n";
					break;
				default:
					break;
				}
			});

		// Start server
		auto res = m_Server->listen();
		if (!res.first)
		{
			std::cerr << "Failed to listen on port " << m_Server->getPort() << ": " << res.second << "\n";
			m_Running.store(false, std::memory_order_release);
			ix::uninitNetSystem();
			return false;
		}

		m_Server->start();
		std::cout << "WebSocket server running on port " << m_Server->getPort() << "\n";

		// Owned rather than detached, so Stop() can wait for it and the process can
		// actually exit instead of being torn down mid-broadcast.
		m_QueueThread = std::thread([this]()
			{
				while (m_Running.load(std::memory_order_acquire))
				{
					ProcessQueue();
					std::this_thread::sleep_for(std::chrono::milliseconds(5));
				}
			});

		return true;
	}

	void WSServer::OnOpen(const std::string& clientId, ix::WebSocket& webSocket, const ix::WebSocketOpenInfo& openInfo)
	{
		const std::string refusal = RefusalReason(openInfo);
		if (!refusal.empty())
		{
			std::cout << "Client refused: " << clientId << " (" << refusal << ")\n";
			webSocket.close(ix::WebSocketCloseConstants::kNormalClosureCode, refusal);
			return;
		}

		// The server owns the socket; take a share of it so a broadcast in flight cannot
		// outlive the connection thread.
		std::shared_ptr<ix::WebSocket> owned;
		for (const auto& client : m_Server->getClients())
		{
			if (client.get() == &webSocket)
			{
				owned = client;
				break;
			}
		}
		if (!owned)
			return;

		std::cout << "Client connected: " << clientId << "\n";
		std::lock_guard<std::mutex> lock(m_Mutex);
		m_Clients[clientId] = std::move(owned);
	}

	std::string WSServer::RefusalReason(const ix::WebSocketOpenInfo& openInfo) const
	{
		const auto origin = openInfo.headers.find("Origin");
		if (origin != openInfo.headers.end() && !origin->second.empty())
		{
			const bool allowed = std::any_of(Protocol::AllowedOrigins.begin(), Protocol::AllowedOrigins.end(),
				[&origin](std::string_view candidate) { return candidate == origin->second; });
			if (!allowed)
				return "Origin not allowed.";
		}

		if (m_Token.empty())
			return {};

		if (!ConstantTimeEquals(m_Token, QueryValue(openInfo.uri, Protocol::TokenParam)))
			return "Invalid or missing token.";

		return {};
	}

	void WSServer::Stop()
	{
		if (!m_Running.exchange(false, std::memory_order_acq_rel))
			return;
		if (m_QueueThread.joinable())
			m_QueueThread.join();
		m_Server->stop();
		{
			std::lock_guard<std::mutex> lock(m_Mutex);
			m_Clients.clear();
		}
		ix::uninitNetSystem();
	}

	void WSServer::SendToClient(const std::string& clientId, const std::string& message)
	{
		std::shared_ptr<ix::WebSocket> socket;
		{
			std::lock_guard<std::mutex> lock(m_Mutex);
			if (auto it = m_Clients.find(clientId); it != m_Clients.end())
				socket = it->second;
		}

		if (socket)
			socket->send(message);
	}

	void WSServer::Broadcast(const std::string& message)
	{
		// The sockets are taken under the lock and written to outside it. Sending a
		// large message takes long enough that holding the lock across it would stall
		// the inbound path, which needs the same lock to check that a frame came from a
		// client we accepted. The shared_ptrs keep every socket alive meanwhile.
		std::vector<std::shared_ptr<ix::WebSocket>> clients;
		{
			std::lock_guard<std::mutex> lock(m_Mutex);
			clients.reserve(m_Clients.size());
			for (const auto& [id, socket] : m_Clients)
				clients.push_back(socket);
		}

		for (const auto& socket : clients)
			socket->send(message);
	}

	void WSServer::ProcessQueue()
	{
		OutboundMessage msg;

		while (MessageQueue::GetInstance().Pop(msg))
		{
			if (msg.ClientId.empty())
				Broadcast(msg.Payload);
			else
				SendToClient(msg.ClientId, msg.Payload);
		}
	}
}
