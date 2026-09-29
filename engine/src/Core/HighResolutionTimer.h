#pragma once
#include <chrono>

namespace Adagio
{
	// A timer belongs to the thread that waits on it: create one per thread.
	class HighResolutionTimer
	{
	public:
		HighResolutionTimer();
		~HighResolutionTimer();

		HighResolutionTimer(const HighResolutionTimer&) = delete;
		HighResolutionTimer& operator=(const HighResolutionTimer&) = delete;

		void SleepFor(std::chrono::microseconds duration);

	private:
		// A Windows HANDLE, kept opaque so windows.h stays out of the headers.
		void* m_Handle = nullptr;
	};
}
