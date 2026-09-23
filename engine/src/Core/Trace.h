#pragma once
#include <atomic>
#include <iostream>
#include <mutex>

namespace Adagio
{
	// Opt-in diagnostic tracing, switched on with --trace
	inline std::atomic<bool> g_Tracing{ false };

	inline void SetTracing(bool enabled) { g_Tracing.store(enabled, std::memory_order_release); }
	inline bool Tracing() { return g_Tracing.load(std::memory_order_acquire); }

	template <typename... Args>
	void Trace(Args&&... args)
	{
		if (!Tracing())
			return;

		static std::mutex mutex;
		std::lock_guard<std::mutex> lock(mutex);
		(std::cout << ... << args) << std::endl;
	}
}
