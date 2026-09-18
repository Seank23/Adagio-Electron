#pragma once
#include "AnalysisStage.h"
#include "AnalysisPipeline.h"
#include "AnalysisUtils.h"

#include <cmath>

namespace Adagio
{
	class NoteDetector : public AnalysisStage
	{
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
				float freq = data[i].Frequency;
				int midi = std::round(69 + 12 * std::log2(freq / A440));
				if (midi < 0 || midi > 127)
					continue;
				int note = ((midi % 12) + 12) % 12;
				int octave = std::floor(midi / 12) - 1;
				float cents = 1200 * std::log2(freq / MidiToFrequency(midi));

				if (std::abs(cents) > errorThreshold)
					continue;

				std::string noteName = NoteNames.at(note) + std::to_string(octave);
				notes.push_back({ noteName, midi, data[i], cents, timestamp });
			}

			auto& rollingNotes = context->PersistentData->RollingNotes;
			rollingNotes.insert(rollingNotes.end(), notes.begin(), notes.end());

			while (!rollingNotes.empty() && std::abs(timestamp - rollingNotes.front().Timestamp) > rollingWindowTime)
				rollingNotes.pop_front();

			context->Notes = std::move(notes);
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
					"min": 5.0,
					"max": 50.0,	
					"default": 20.0
				}
			})");
		}

	private:
		static constexpr float A440 = 440.0f;

		float MidiToFrequency(int midi) const
		{
			return A440 * std::pow(2.0f, (midi - 69) / 12.0f);
		}
	};
}
