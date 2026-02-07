#pragma once
#include <memory>
#include <thread>

namespace Adagio
{
	struct AnalysisParams
	{
		float SampleRate = 44100.0f;
		int FrameLength = 8192;
	};
	struct AnalysisResult;

	class AudioDecoder;
	class AnalysisPipeline;
	class AudioData;

	template<typename T>
	class RingBuffer;

	class AnalysisService
	{
	public:
		AnalysisService();
		~AnalysisService();

		void Init(std::shared_ptr<AudioDecoder> decoder, AnalysisParams params);
		void Reset();
		void StartAnalysis();
		void StopAnalysis();
		void RequestCurrentFrameAnalysis();

		void SetIntervalMs(int intervalMs) { m_IntervalMs = intervalMs; }

	private:
		std::unique_ptr<AnalysisResult> ProcessCurrentFrame();

		std::thread m_AnalysisThread;
		std::shared_ptr<AudioDecoder> m_Decoder;
		std::unique_ptr<AnalysisPipeline> m_Pipeline;
		std::shared_ptr<AudioData> m_AudioSource;
		RingBuffer<float>* m_AnalysisBuffer;
		AnalysisParams m_Params;
		int m_IntervalMs;
		std::atomic<bool> m_Running{ false };
	};
}

