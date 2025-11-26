#pragma once
#include <queue>
#include <mutex>
#include <string>

namespace Adagio
{
	class MessageQueue
	{
	public:
		static MessageQueue& Instance()
		{
			static MessageQueue inst;
			return inst;
		}

		void Push(const std::string& msg)
		{
			std::lock_guard<std::mutex> lock(m_Mutex);
			m_Queue.push(msg);
		}

		bool Pop(std::string& outMsg)
		{
			std::lock_guard<std::mutex> lock(m_Mutex);
			if (m_Queue.empty()) return false;

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