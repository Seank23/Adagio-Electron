#pragma once
#include <kfr/base/univector.hpp>

#include <atomic>
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
	class PlaybackService;

	template<typename T>
	class RingBuffer;

	class AnalysisService
	{
	public:
		AnalysisService();
		~AnalysisService();

		void Init(std::shared_ptr<AudioDecoder> decoder, AnalysisParams params, const PlaybackService* playback);
		void Reset();
		void StartAnalysis();
		void StopAnalysis();
		void RequestCurrentFrameAnalysis();

		void SetIntervalMs(int intervalMs) { m_IntervalMs = intervalMs; }

		// Where the playhead is now, in source seconds: the position the audio callback last published
		static double ExtrapolatePlayhead(double playbackTime, double lastFrameTimestamp, double now, double speed, bool playing);

	private:
		std::unique_ptr<AnalysisResult> ProcessCurrentFrame();
		void PublishCurrentFrame();
		bool SyncSeekGeneration();
		void PreprocessStream(kfr::univector<float>& outStream);

		std::thread m_AnalysisThread;
		std::shared_ptr<AudioDecoder> m_Decoder;
		const PlaybackService* m_Playback = nullptr;
		std::unique_ptr<AnalysisPipeline> m_Pipeline;
		std::shared_ptr<AudioData> m_AudioSource;
		std::unique_ptr<RingBuffer<float>> m_AnalysisBuffer;
		AnalysisParams m_Params;
		int m_IntervalMs;
		int m_RollingAvgCount;
		std::atomic<bool> m_Running{ false };
		double m_AnalysisTimestamp = 0.0;

		uint32_t m_SeekGenerationSeen = 0;
	};
}
