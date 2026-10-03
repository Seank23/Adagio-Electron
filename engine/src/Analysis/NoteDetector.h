#pragma once
#include "AnalysisStage.h"
#include "AnalysisPipeline.h"
#include "AnalysisUtils.h"

#include <cmath>

namespace Adagio
{
	struct NoteDetectorSettings
	{
		float ERROR_THRESHOLD = 25.0f;
		float A440_TUNING = 440.0f;

		NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(NoteDetectorSettings, ERROR_THRESHOLD, A440_TUNING)
	};

	class NoteDetector : public ConfigurableStage<NoteDetectorSettings>
	{
	public:
		static constexpr std::string_view Name = "NoteDetector";
		std::string_view GetStageName() const override { return Name; }

		virtual void Execute(AnalysisContext* context) override
		{
			auto& data = context->Peaks;

			const float errorThreshold = m_Settings.ERROR_THRESHOLD;
			const float A440 = m_Settings.A440_TUNING;
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
				float cents = 1200 * std::log2(freq / MidiToFrequency(midi, A440));

				if (std::abs(cents) > errorThreshold)
					continue;

				std::string noteName = NoteNames.at(note) + std::to_string(octave);
				notes.push_back({ noteName, midi, data[i], cents, timestamp });
			}

			context->Notes = std::move(notes);
		}

		virtual AnalysisStageType GetType() const override
		{
			return AnalysisStageType::FeatureExtractor;
		}

	protected:
		virtual nlohmann::json BuildSettingsSchema() const override
		{
			return nlohmann::json::parse(R"({
				"ERROR_THRESHOLD": {
					"name": "Error Threshold",
					"type": "float",	
					"min": 0.0,
					"max": 50.0
				},
				"A440_TUNING": {
					"name": "A440 Reference",
					"type": "float",	
					"min": 415.0,
					"max": 466.0
				}
			})");
		}

	private:
		float MidiToFrequency(int midi, float tuningReference = 440.0f) const
		{
			return tuningReference * std::pow(2.0f, (midi - 69) / 12.0f);
		}
	};
}
