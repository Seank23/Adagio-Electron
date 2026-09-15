#pragma once
#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

namespace RubberBand
{
	class RubberBandStretcher;
}

namespace Adagio
{
	template<typename T>
	class RingBuffer;

	class TimeProcessor
	{
	public:
		TimeProcessor();
		~TimeProcessor();

		void Init(int sampleRate, int channels, RingBuffer<float>* buffer, size_t maxFrames = 512);
		void Reset();

		// Audio thread only.
		size_t ProcessAudio(float* output, size_t samplesRequested);
		void ResetStretcher();

		void SetSpeed(float speed);
		double GetSpeed() const { return m_PendingSpeed.load(std::memory_order_acquire); }

	private:
		std::atomic<double> m_PendingSpeed{ 1.0 };
		std::atomic<double> m_AppliedSpeed{ 1.0 };
		std::atomic<bool> m_ResetRequested{ false };

		std::unique_ptr<RubberBand::RubberBandStretcher> m_Stretcher;
		RingBuffer<float>* m_InputBuffer = nullptr;
		int m_Channels = 0;

		std::vector<float> m_InputBufferInterleaved;

		std::vector<std::vector<float>> m_InputChannels;
		std::vector<std::vector<float>> m_OutputChannels;

		std::vector<float*> m_InputPtrs;
		std::vector<float*> m_OutputPtrs;

		size_t m_MaxFramesPerBlock = 0;
		size_t m_OutputCapacityFrames = 0;

		bool m_InStretchMode = false;
		int64_t m_LatencyToDiscard = 0;
	};
}
