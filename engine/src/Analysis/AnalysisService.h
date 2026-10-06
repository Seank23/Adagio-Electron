#pragma once
#include "../Core/EngineParams.h"

#include <kfr/base/univector.hpp>

#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

namespace Adagio
{
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
		void SetParams(const AnalysisParams& params);
		void Reset();
		void StartAnalysis();
		void StopAnalysis();
		void RequestCurrentFrameAnalysis();
		void ResetAnalysis();

		nlohmann::json GetSchemaJson() const;
		bool SetSetting(const std::string& stage, const std::string& key, const nlohmann::json& value, std::string& outError);

		const AnalysisParams& GetParams() const { return m_Params; }

		static double ExtrapolatePlayhead(double playbackTime, double lastFrameTimestamp, double now, double speed, bool playing);
		static std::unique_ptr<AnalysisPipeline> CreatePipeline();
		static kfr::univector<float> Preprocess(const AudioData& source, float sampleRate);

		static nlohmann::json ParamsJson(const AnalysisParams& params);

	private:
		// A stall longer than this is a hiccup, not audio heard; it shouldn't outweigh the history.
		static constexpr double MaxDeltaSeconds = 0.25;
		// Frames arrive at sampleRate / hopSize a second, so the analysis event goes by wall time: 31.25 Hz at any setting.
		static constexpr std::chrono::microseconds AnalysisEventInterval{ 32000 };

		void BuildPipeline();
		void LoadAnalysisBuffer();
		double EstimatePlayhead() const;
		std::unique_ptr<AnalysisResult> ProcessFrameAt(double sourceSeconds, double deltaTime);
		void PublishCurrentFrame();
		void PublishSpectrum(const AnalysisResult& result);
		bool SyncSeekGeneration();

		std::thread m_AnalysisThread;
		std::shared_ptr<PcmFeeder> m_Feeder;
		const PlaybackService* m_Playback = nullptr;
		std::unique_ptr<AnalysisPipeline> m_Pipeline;
		std::shared_ptr<AudioData> m_AudioSource;
		std::unique_ptr<RingBuffer<float>> m_AnalysisBuffer;

		AnalysisParams m_Params;

		std::atomic<bool> m_Running{ false };
		std::atomic<bool> m_ResetRequested{ false };

		uint32_t m_SeekGenerationSeen = 0;
		double m_LastAnalysisStreamPos = 0.0;
	};
}
