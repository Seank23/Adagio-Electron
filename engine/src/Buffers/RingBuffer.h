#pragma once
#include <atomic>
#include <cstdint>
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
			if (free == 0)
				return 0;

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

		size_t Read(T* outData, size_t count, size_t readFrom = -1)
		{
			size_t available = GetAvailableCount();
			if (available == 0)
				return 0;

			size_t toRead = (count > available) ? available : count;
			size_t readVal = readFrom != -1 ? readFrom : m_ReadIndex.load(std::memory_order_relaxed);

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

		// Writer only. Tag and index share one atomic so a reader can't mix two marks; indices must fit in 32 bits.
		void MarkWritePosition(uint32_t tag)
		{
			const uint64_t index = m_WriteIndex.load(std::memory_order_relaxed);
			m_Mark.store((static_cast<uint64_t>(tag) << 32) | index, std::memory_order_release);
		}

		// Reader only.
		bool DropToMark(uint32_t tag)
		{
			const uint64_t mark = m_Mark.load(std::memory_order_acquire);
			if (static_cast<uint32_t>(mark >> 32) != tag)
				return false;

			// Never move back: the reader may already have consumed past the mark.
			const size_t markIndex = static_cast<size_t>(mark & 0xFFFFFFFFu);
			const size_t readVal = m_ReadIndex.load(std::memory_order_relaxed);
			const size_t writeVal = m_WriteIndex.load(std::memory_order_acquire);
			const size_t toMark = (markIndex + m_Capacity - readVal) % m_Capacity;
			const size_t toWrite = (writeVal + m_Capacity - readVal) % m_Capacity;
			if (toMark <= toWrite)
				m_ReadIndex.store(markIndex, std::memory_order_release);
			return true;
		}

		void Clear()
		{
			m_WriteIndex.store(0, std::memory_order_release);
			m_ReadIndex.store(0, std::memory_order_release);
			m_Mark.store(NO_MARK, std::memory_order_release);
		}

	private:
		static constexpr uint64_t NO_MARK = UINT64_MAX;

		size_t m_Capacity;
		std::vector<T> m_Buffer;
		std::atomic<size_t> m_WriteIndex;
		std::atomic<size_t> m_ReadIndex;
		std::atomic<uint64_t> m_Mark{ NO_MARK };
	};
}
