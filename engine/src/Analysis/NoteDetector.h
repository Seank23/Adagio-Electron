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
		float TUNING_WINDOW = 1.0f;

		NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(NoteDetectorSettings, ERROR_THRESHOLD, A440_TUNING, TUNING_WINDOW)
	};

	class NoteDetector : public ConfigurableStage<NoteDetectorSettings>
	{
	private:
		static constexpr double MIN_TUNING_WEIGHT = 0.05;

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
			double errorSum = 0.0;
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
				errorSum += cents;
			}

			context->AvgTuningError = AccumulateTuningError(*context->PersistentData, errorSum, notes.size(), context->Frame.DeltaTime);
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
				},
				"TUNING_WINDOW": {
					"name": "Tuning Error Window",
					"type": "float",
					"min": 0.5,
					"max": 5.0
				}
			})");
		}

	private:
		std::optional<float> AccumulateTuningError(PersistentData& persistent, double errorSum, size_t noteCount, double dt)
		{
			const float reference = m_Settings.A440_TUNING;
			if (persistent.TuningReference > 0.0f && persistent.TuningReference != reference)
				persistent.TuningErrorSum -= 1200.0 * std::log2((double)reference / persistent.TuningReference) * persistent.TuningErrorWeight;
			persistent.TuningReference = reference;

			const double decay = std::exp(-dt / (m_Settings.TUNING_WINDOW / 3.0));
			persistent.TuningErrorSum = persistent.TuningErrorSum * decay + errorSum * dt;
			persistent.TuningErrorWeight = persistent.TuningErrorWeight * decay + (double)noteCount * dt;

			if (persistent.TuningErrorWeight < MIN_TUNING_WEIGHT)
				return std::nullopt;
			return (float)(persistent.TuningErrorSum / persistent.TuningErrorWeight);
		}

		float MidiToFrequency(int midi, float tuningReference = 440.0f) const
		{
			return tuningReference * std::pow(2.0f, (midi - 69) / 12.0f);
		}
	};
}
