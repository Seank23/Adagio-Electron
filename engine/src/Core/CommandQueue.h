#pragma once
#include "Command.h"
#include <queue>
#include <mutex>

namespace Adagio
{
	class CommandQueue
	{
	public:
		static CommandQueue& Instance()
		{
			static CommandQueue inst;
			return inst;
		}

		void Push(const Command& cmd)
		{
			std::lock_guard<std::mutex> lock(m_Mutex);
			m_Queue.push(cmd);
		}

		bool Pop(Command& outCmd)
		{
			std::lock_guard<std::mutex> lock(m_Mutex);
			if (m_Queue.empty()) return false;

			outCmd = m_Queue.front();
			m_Queue.pop();
			return true;
		}

	private:
		CommandQueue() = default;

		std::queue<Command> m_Queue;
		std::mutex m_Mutex;
	};
}