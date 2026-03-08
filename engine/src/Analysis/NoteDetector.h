#pragma once
#include "AnalysisStage.h"
#include "AnalysisPipeline.h"

namespace Adagio
{
	class NoteDetector : public AnalysisStage
	{
	private:
		const float C0 = 16.3516f;

		std::map<int, std::string> NoteNames = {
			{0, "C"},
			{1, "Db"},
			{2, "D"},
			{3, "Eb"},
			{4, "E"},
			{5, "F"},
			{6, "Gb"},
			{7, "G"},
			{8, "Ab"},
			{9, "A"},
			{10, "Bb"},
			{11, "B"}
		};

	public:
		virtual void Execute(AnalysisContext* context) override
		{
			AnalysisStage::Execute(context);
			auto& data = context->Peaks;
			nlohmann::json settings = context->Settings;

			const float errorThreshold = GetSetting<float>(settings, "ERROR_THRESHOLD");
			const float rollingWindowTime = GetSetting<float>(settings, "ROLLING_WINDOW");
			const double timestamp = context->Frame.Timestamp;

			std::vector<Note> notes;
			for (size_t i = 0; i < data.size(); i++)
			{
				float frequencyRatio = std::log2(data[i].Frequency / C0);
				int note = static_cast<int>(std::round(frequencyRatio * 12)) % 12;
				int octave = static_cast<int>(std::floor(frequencyRatio));
				float error = 12.0f * (frequencyRatio - octave) - note;
				error *= 100.0f; // Convert to cents

				if (std::abs(error) > errorThreshold)
					continue;

				std::string noteName = NoteNames.at(note) + std::to_string(octave);
				notes.push_back({ noteName, data[i], error, timestamp });
			}

			auto& rollingNotes = context->PersistentData->RollingNotes;
			rollingNotes.insert(rollingNotes.end(), notes.begin(), notes.end());
			for (size_t i = 0; i < rollingNotes.size();)
			{
				if (timestamp - rollingNotes[i].Timestamp > rollingWindowTime)
					rollingNotes.erase(rollingNotes.begin() + i);
				else
					i++;
			}

			std::map<int, float> noteScores;
			for (const auto& note : rollingNotes)
			{
				int roundedFrequency = std::round(note.PeakInfo.Frequency);
				noteScores[roundedFrequency] += note.PeakInfo.Score;
			}
			context->Notes = std::move(notes);
			context->NoteScores = std::move(noteScores);
		}

		virtual AnalysisStageType GetType() const override
		{
			return AnalysisStageType::FeatureExtractor;
		}

		virtual nlohmann::json GetSettings() const override
		{
			return nlohmann::json::parse(R"({
				"ERROR_THRESHOLD": {
					"name": "Error Threshold",
					"type": "float",	
					"min": 0.0,
					"max": 50.0,	
					"default": 25.0
				},
				"ROLLING_WINDOW": {
					"name": "Rolling Window",
					"type": "float",	
					"min": 0.1,
					"max": 50.0,	
					"default": 10.0
				}
			})");
		}
	};
}
