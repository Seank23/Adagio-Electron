#pragma once
#include <deque>
#include <vector>
#include <memory>
#include "AudioFrame.h"

#include <nlohmann/json.hpp>

namespace Adagio
{
	class AnalysisStage;

	struct Peak
	{
		float Frequency = 0.0f;
		float Magnitude = 0.0f;
		float Score = 0.0f;
	};

	struct Note
	{
		std::string Name;
		int Midi = 0;
		Peak PeakInfo;
		float ErrorCents = 0.0f;
		double Timestamp = 0.0;
	};

	struct Chord
	{
		std::string Name;
		std::string Root;
		std::string Quality;
		std::vector<Note> Notes;
		int RootOccurences = 0;
		int NumExtentions = 0;
		int FifthOmitted = 0;
		double Probability = 0.0;
	};

	struct PersistentData
	{
		std::deque<Note> RollingNotes;
		Chord PreviousChord;
	};

	struct AnalysisContext
	{
		const AudioFrame& Frame;
		PersistentData* PersistentData = nullptr;

		kfr::univector<float> Samples;
		kfr::univector<float> Windowed;
		kfr::univector<kfr::complex<float>> Spectrum;
		kfr::univector<float> Magnitudes;
		std::vector<Peak> Peaks;
		float BinHz;
		int Harmonics = 0;

		std::vector<Note> Notes;
		std::map<int, float> KeyFrequencyHistogram;
		std::map<int, float> ChordFrequencyHistogram;
		std::vector<Chord> PredictedChords;
		std::string DetectedKey;

		nlohmann::json Settings;
	};

	struct AnalysisResult
	{
		std::unique_ptr<AnalysisContext> Context;
		float MaxMagnitude = 0.0f;
		float SampleRate = 0.0f;
		float ExecutionTimeMs = 0.0f;
	};

	class AnalysisPipeline
	{
	public:
		AnalysisPipeline();
		~AnalysisPipeline();

		void AddStage(std::unique_ptr<AnalysisStage> stage);
		std::unique_ptr<AnalysisResult> ProcessFrame(const AudioFrame& frame);

		void ResetPersistentData();

		static nlohmann::json GetResultJson(const AnalysisResult& result);

	private:
		std::vector<std::unique_ptr<AnalysisStage>> m_Stages;
		nlohmann::json m_Settings;

		std::unique_ptr<PersistentData> m_PersistentData;
	};
}
