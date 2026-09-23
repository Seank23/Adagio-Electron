#pragma once
#include <mutex>
#include <queue>
#include <string>
#include <utility>

namespace Adagio
{
	struct OutboundMessage
	{
		std::string Payload;
		// Empty means every client
		std::string ClientId;
	};

	class MessageQueue
	{
	public:
		MessageQueue(const MessageQueue&) = delete;
		MessageQueue& operator=(const MessageQueue&) = delete;

		static MessageQueue& GetInstance()
		{
			static MessageQueue instance;
			return instance;
		}

		void Push(std::string msg)
		{
			std::lock_guard<std::mutex> lock(m_Mutex);
			m_Queue.push({ std::move(msg), {} });
		}

		void PushTo(std::string clientId, std::string msg)
		{
			std::lock_guard<std::mutex> lock(m_Mutex);
			m_Queue.push({ std::move(msg), std::move(clientId) });
		}

		bool Pop(OutboundMessage& outMsg)
		{
			std::lock_guard<std::mutex> lock(m_Mutex);
			if (m_Queue.empty())
				return false;

			outMsg = std::move(m_Queue.front());
			m_Queue.pop();
			return true;
		}

	private:
		MessageQueue() = default;

		std::queue<OutboundMessage> m_Queue;
		std::mutex m_Mutex;
	};
}
