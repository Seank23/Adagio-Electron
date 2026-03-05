#pragma once
#include <vector>
#include <memory>
#include "AudioFrame.h"

#include <nlohmann/json.hpp>

namespace Adagio
{
	class AnalysisStage;

	struct Peak
	{
		float Frequency;
		float Magnitude;
		float Score;
	};

	struct Note
	{
		std::string Name;
		Peak PeakInfo;
		float ErrorCents;
		double Timestamp;
	};

	struct PersistentData
	{
		std::vector<Note> RollingNotes;
	};

	struct AnalysisContext
	{
		const AudioFrame& Frame;
		PersistentData* PersistentData;

		kfr::univector<float> Samples;
		kfr::univector<float> Windowed;
		kfr::univector<kfr::complex<float>> Spectrum;
		kfr::univector<float> Magnitudes;
		std::vector<Peak> Peaks;
		std::vector<std::pair<size_t, float>> LocalMedian;
		std::vector<Note> Notes;
		std::map<int, float> NoteScores;

		nlohmann::json Settings;
	};

	struct AnalysisResult
	{
		std::unique_ptr<AnalysisContext> Context;
		float MaxMagnitude;
		float SampleRate;
		float ExecutionTimeMs;
	};

	class AnalysisPipeline
	{
	public:
		AnalysisPipeline();
		~AnalysisPipeline();

		void AddStage(std::unique_ptr<AnalysisStage> stage);
		std::unique_ptr<AnalysisResult> ProcessFrame(const AudioFrame& frame);

		static nlohmann::json GetResultJson(const AnalysisResult& result);

	private:
		std::vector<std::unique_ptr<AnalysisStage>> m_Stages;
		nlohmann::json m_Settings;

		std::unique_ptr<PersistentData> m_PersistentData;
	};
}

