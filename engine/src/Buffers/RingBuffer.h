#pragma once
#include <atomic>
#include <vector>
#include <cstring>

namespace Adagio
{
	template <typename T>
	class RingBuffer
	{
	public:
		explicit RingBuffer(size_t capacity)
			: m_Capacity(capacity), m_Buffer(capacity), m_WriteIndex(0), m_ReadIndex(0)
		{
		}

		size_t GetCapacity() const { return m_Capacity; }
		size_t GetFreeCapacity() const { return m_Capacity - GetAvailableCount() - 1; }
		size_t GetAvailableCount() const
		{
			size_t writeVal = m_WriteIndex.load(std::memory_order_acquire);
			size_t readVal = m_ReadIndex.load(std::memory_order_acquire);
			return (writeVal + m_Capacity - readVal) % m_Capacity;
		}

		size_t Write(const T* data, size_t count)
		{
			size_t free = GetFreeCapacity();
			if (free == 0) return 0;

			size_t toWrite = (count > free) ? free : count;
			size_t writeVal = m_WriteIndex.load(std::memory_order_relaxed);

			size_t right = m_Capacity - writeVal;
			if (toWrite <= right)
			{
				std::memcpy(m_Buffer.data() + writeVal, data, toWrite * sizeof(T));
			}
			else
			{
				std::memcpy(m_Buffer.data() + writeVal, data, right * sizeof(T));
				std::memcpy(m_Buffer.data(), data + right, (toWrite - right) * sizeof(T));
			}
			m_WriteIndex.store((writeVal + toWrite) % m_Capacity, std::memory_order_release);
			return toWrite;
		}

		size_t Read(T* outData, size_t count)
		{
			size_t available = GetAvailableCount();
			if (available == 0) return 0;

			size_t toRead = (count > available) ? available : count;
			size_t readVal = m_ReadIndex.load(std::memory_order_relaxed);

			size_t right = m_Capacity - readVal;
			if (toRead <= right)
			{
				std::memcpy(outData, m_Buffer.data() + readVal, toRead * sizeof(T));
			}
			else
			{
				std::memcpy(outData, m_Buffer.data() + readVal, right * sizeof(T));
				std::memcpy(outData + right, m_Buffer.data(), (toRead - right) * sizeof(T));
			}
			m_ReadIndex.store((readVal + toRead) % m_Capacity, std::memory_order_release);
			return toRead;
		}

		void Clear()
		{
			m_WriteIndex.store(0, std::memory_order_release);
			m_ReadIndex.store(0, std::memory_order_release);
			m_Buffer.clear();
		}

	private:
		size_t m_Capacity;
		std::vector<T> m_Buffer;
		std::atomic<size_t> m_WriteIndex;
		std::atomic<size_t> m_ReadIndex;
	};
}