#pragma once
#include <atomic>
#include <cstdint>
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
		// A binary frame travels in the same string, only the WebSocket opcode differs.
		bool Binary = false;
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
			m_Queue.push({ std::move(msg), {}, false });
		}

		void PushTo(std::string clientId, std::string msg)
		{
			std::lock_guard<std::mutex> lock(m_Mutex);
			m_Queue.push({ std::move(msg), std::move(clientId), false });
		}

		void PushBinary(std::string frame)
		{
			std::lock_guard<std::mutex> lock(m_Mutex);
			m_Queue.push({ std::move(frame), {}, true });
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

		void AddBytesSent(uint64_t bytes) { m_BytesSent.fetch_add(bytes, std::memory_order_relaxed); }
		uint64_t GetBytesSent() const { return m_BytesSent.load(std::memory_order_relaxed); }

	private:
		MessageQueue() = default;

		std::queue<OutboundMessage> m_Queue;
		std::mutex m_Mutex;
		std::atomic<uint64_t> m_BytesSent{ 0 };
	};
}
