#include "HighResolutionTimer.h"

#include <thread>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// Windows 10 1803 and later. Older SDKs don't name the flag.
#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif
#endif

namespace Adagio
{
	HighResolutionTimer::HighResolutionTimer()
	{
#ifdef _WIN32
		m_Handle = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
#endif
	}

	HighResolutionTimer::~HighResolutionTimer()
	{
#ifdef _WIN32
		if (m_Handle)
			CloseHandle(m_Handle);
#endif
	}

	void HighResolutionTimer::SleepFor(std::chrono::microseconds duration)
	{
		if (duration.count() <= 0)
			return;

#ifdef _WIN32
		if (m_Handle)
		{
			// Negative means relative, in 100 ns units.
			LARGE_INTEGER due;
			due.QuadPart = -(LONGLONG)duration.count() * 10;
			if (SetWaitableTimerEx(m_Handle, &due, 0, nullptr, nullptr, nullptr, 0))
			{
				WaitForSingleObject(m_Handle, INFINITE);
				return;
			}
		}
#endif
		// No high-resolution timer on this system: the tick-bound sleep still works, just coarsely.
		std::this_thread::sleep_for(duration);
	}
}
