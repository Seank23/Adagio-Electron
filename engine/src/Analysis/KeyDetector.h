#pragma once
#include "AnalysisStage.h"
#include "AnalysisPipeline.h"
#include "AnalysisUtils.h"

namespace Adagio
{
	struct KeyDetectorSettings
	{
		float ROLLING_WINDOW = 20.0f;

		NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(KeyDetectorSettings, ROLLING_WINDOW)
	};

	class KeyDetector : public ConfigurableStage<KeyDetectorSettings>
	{
	public:
		static constexpr std::string_view Name = "KeyDetector";
		std::string_view GetStageName() const override { return Name; }

	private:
		const std::array<double, 12> KrumhanslMajor = {
			6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f,
			2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f
		};
		const std::array<double, 12> KrumhanslMinor = {
			6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f,
			2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f
		};

		const std::map<int, std::array<int, 7>> Scales = {
			{ 0, {0, 2, 4, 5, 7, 9, 11} }, // C major
			{ 1, {1, 3, 5, 6, 8, 10, 0} }, // C# major
			{ 2, {2, 4, 6, 7, 9, 11, 1} }, // D major
			{ 3, {3, 5, 7, 8, 10, 0, 2} }, // D# major
			{ 4, {4, 6, 8, 9, 11, 1, 3} }, // E major
			{ 5, {5, 7, 9, 10, 0, 2, 4} }, // F major
			{ 6, {6, 8, 10, 11, 1, 3, 5} }, // F# major
			{ 7, {7, 9, 11, 0, 2, 4, 6} }, // G major
			{ 8, {8, 10, 0, 1, 3, 5 ,7} }, // G# major
			{ 9, {9 ,11 ,1 ,2 ,4 ,6 ,8 } }, // A major
			{ 10, {10 ,0 ,2 ,3 ,5 ,7 ,9 } }, // A# major
			{ 11, {11 ,1 ,3 ,4 ,6 ,8 ,10} } // B major
		};

	public:
		virtual void Execute(AnalysisContext* context) override
		{
			const double dt = context->Frame.DeltaTime;
			const double decay = std::exp(-dt / (m_Settings.ROLLING_WINDOW / 3.0));

			auto& frequencyAccumulator = context->PersistentData->FreqAccumulator;
			auto& noteClassAccumulator = context->PersistentData->NoteClassAccumulator;

			for (auto& value : noteClassAccumulator)
				value *= decay;

			for (auto it = frequencyAccumulator.begin(); it != frequencyAccumulator.end();)
			{
				it->second *= (float)decay;
				if (it->second < PruneThreshold)
					it = frequencyAccumulator.erase(it);
				else
					++it;
			}

			for (const auto& note : context->Notes)
			{
				const double weight = note.PeakInfo.Score * dt;
				frequencyAccumulator[(int)std::round(note.PeakInfo.Frequency)] += static_cast<float>(weight);
				noteClassAccumulator[note.Midi % 12] += weight;
			}

			context->KeyFrequencyHistogram = frequencyAccumulator;

			std::array<double, 12> notesHistogram = noteClassAccumulator;
			double sum = std::reduce(notesHistogram.begin(), notesHistogram.end(), 0.0);
			// No notes means no key
			if (sum <= 0.0)
			{
				context->PredictedKey = KeySignature{};
				return;
			}
			for (auto& val : notesHistogram)
				val /= sum;

			std::array<double, 12> similarity{};
			FindTotalScaleSimilarity(notesHistogram, similarity);

			double maxSimilarity = 0.0;
			int bestKey = 0;
			for (int i = 0; i < 12; i++)
			{
				if (similarity[i] > maxSimilarity)
				{
					maxSimilarity = similarity[i];
					bestKey = i;
				}
			}

			KeySignature keySig;
			std::array<int, 7> keyScale = Scales.at(bestKey);

			int relativeMinorIndex = keyScale[5]; // The 6th note in the major scale is the root of the relative minor
			if (notesHistogram[relativeMinorIndex] > notesHistogram[bestKey])
			{
				keySig.Tonic = NoteNames.at(relativeMinorIndex);
				keySig.TonicClass = relativeMinorIndex;
				keySig.ModeName = "Minor";
			}
			else
			{
				keySig.Tonic = NoteNames.at(bestKey);
				keySig.TonicClass = bestKey;
				keySig.ModeName = "Major";
			}

			keySig.Name = keySig.Tonic + " " + keySig.ModeName;
			keySig.Scale = keyScale;

			context->PredictedKey = std::move(keySig);
		}

		virtual AnalysisStageType GetType() const override
		{
			return AnalysisStageType::FeatureExtractor;
		}

	protected:
		virtual nlohmann::json BuildSettingsSchema() const override
		{
			return nlohmann::json::parse(R"({
				"ROLLING_WINDOW": {
					"name": "Rolling Window",
					"type": "float",
					"min": 5.0,
					"max": 50.0
				}
			})");
		}

	private:
		static constexpr float PruneThreshold = 1e-6f;

		void FindTotalScaleSimilarity(const std::array<double, 12>& histogram, std::array<double, 12>& outSimilarity)
		{
			for (int i = 0; i < 12; i++)
			{
				std::array<int, 7> scale = Scales.at(i);
				for (int j = 0; j < 7; j++)
				{
					outSimilarity[i] += histogram[scale[j]];
				}
			}
		}
	};
}
