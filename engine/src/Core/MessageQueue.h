#pragma once
#include <mutex>
#include <queue>
#include <string>

namespace Adagio
{
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

		void Push(const std::string& msg)
		{
			std::lock_guard<std::mutex> lock(m_Mutex);
			m_Queue.push(msg);
		}

		bool Pop(std::string& outMsg)
		{
			std::lock_guard<std::mutex> lock(m_Mutex);
			if (m_Queue.empty())
				return false;

			outMsg = m_Queue.front();
			m_Queue.pop();
			return true;
		}

	private:
		MessageQueue() = default;

		std::queue<std::string> m_Queue;
		std::mutex m_Mutex;
	};
}
