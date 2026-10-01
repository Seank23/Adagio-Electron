#pragma once
#include <kfr/base/univector.hpp>

#include <nlohmann/json.hpp>

#include <atomic>
#include <memory>
#include <thread>

namespace Adagio
{
	struct AnalysisParams
	{
		float SampleRate = 8000.0f;
		int FrameLength = 8192;
		int HopSize = 256;
	};
	struct AnalysisResult;

	class PcmFeeder;
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

		void Init(std::shared_ptr<PcmFeeder> feeder, AnalysisParams params, const PlaybackService* playback);
		void Reset();
		void StartAnalysis();
		void StopAnalysis();
		void RequestCurrentFrameAnalysis();

		nlohmann::json GetSchemaJson() const;
		bool SetSetting(const std::string& stage, const std::string& key, const nlohmann::json& value, std::string& outError);

		static double ExtrapolatePlayhead(double playbackTime, double lastFrameTimestamp, double now, double speed, bool playing);
		static std::unique_ptr<AnalysisPipeline> CreatePipeline();

	private:
		// A stall longer than this is a hiccup, not audio heard; it shouldn't outweigh the history.
		static constexpr double MaxDeltaSeconds = 0.25;
		static constexpr int FramesPerAnalysisEvent = 4;

		void BuildPipeline();
		double EstimatePlayhead() const;
		std::unique_ptr<AnalysisResult> ProcessFrameAt(double sourceSeconds, double deltaTime);
		void PublishCurrentFrame();
		void PublishSpectrum(const AnalysisResult& result);
		bool SyncSeekGeneration();
		void PreprocessStream(kfr::univector<float>& outStream);

		std::thread m_AnalysisThread;
		std::shared_ptr<PcmFeeder> m_Feeder;
		const PlaybackService* m_Playback = nullptr;
		std::unique_ptr<AnalysisPipeline> m_Pipeline;
		std::shared_ptr<AudioData> m_AudioSource;
		std::unique_ptr<RingBuffer<float>> m_AnalysisBuffer;
		AnalysisParams m_Params;
		int m_RollingAvgCount;
		std::atomic<bool> m_Running{ false };

		uint32_t m_SeekGenerationSeen = 0;
		double m_LastAnalysisStreamPos = 0.0;
	};
}
