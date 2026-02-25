#pragma once
#include <atomic>
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

		size_t ProcessAudio(float* output, size_t samplesRequested);

		void SetSpeed(float speed);
		double GetSpeed() const { return m_Speed.load(std::memory_order_acquire); }

	private:
		std::atomic<double> m_Speed{ 1.0f };

		std::unique_ptr<RubberBand::RubberBandStretcher> m_Stretcher;
		RingBuffer<float>* m_InputBuffer;
		int m_Channels;

		std::vector<float> m_InputBufferInterleaved;
		std::vector<float> m_OutputBufferInterleaved;

		std::vector<std::vector<float>> m_InputChannels;
		std::vector<std::vector<float>> m_OutputChannels;

		std::vector<float*> m_InputPtrs;
		std::vector<float*> m_OutputPtrs;

		size_t m_MaxFramesPerBlock;
		bool m_FirstStretch = true;
	};
}