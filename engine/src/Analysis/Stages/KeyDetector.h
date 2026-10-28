#pragma once
#include "../AnalysisStage.h"
#include "../AnalysisPipeline.h"
#include "../AnalysisUtils.h"

#include <cmath>
#include <limits>
#include <numeric>

namespace Adagio
{
	struct KeyDetectorSettings
	{
		float ROLLING_WINDOW = 20.0f;
		std::string ALGORITHM = "Krumhansl";

		NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(KeyDetectorSettings, ROLLING_WINDOW, ALGORITHM)
	};

	class KeyDetector : public ConfigurableStage<KeyDetectorSettings>
	{
	private:
		static constexpr float PRUNE_THRESHOLD = 1e-6f;
		static constexpr double MIN_HISTOGRAM_VARIANCE = 1e-12;

		// Krumhansl-Kessler probe-tone ratings, indexed by semitones above the tonic.
		static constexpr std::array<double, 12> KrumhanslMajor = {
			6.35, 2.23, 3.48, 2.33, 4.38, 4.09,
			2.52, 5.19, 2.39, 3.66, 2.29, 2.88
		};
		static constexpr std::array<double, 12> KrumhanslMinor = {
			6.33, 2.68, 3.52, 5.38, 2.60, 3.53,
			2.54, 4.75, 3.98, 2.69, 3.34, 3.17
		};

		const std::map<int, std::array<int, 7>> Scales = {
			{ 0,{ 0, 2, 4, 5, 7, 9, 11 } }, // C major
			{ 1,{ 1, 3, 5, 6, 8, 10, 0 } }, // C# major
			{ 2,{ 2, 4, 6, 7, 9, 11, 1 } }, // D major
			{ 3,{ 3, 5, 7, 8, 10, 0, 2 } }, // D# major
			{ 4,{ 4, 6, 8, 9, 11, 1, 3 } }, // E major
			{ 5,{ 5, 7, 9, 10, 0, 2, 4 } }, // F major
			{ 6,{ 6, 8, 10, 11, 1, 3, 5 } }, // F# major
			{ 7,{ 7, 9, 11, 0, 2, 4, 6 } }, // G major
			{ 8,{ 8, 10, 0, 1, 3, 5 ,7 } }, // G# major
			{ 9,{ 9 ,11 ,1 ,2 ,4 ,6 ,8 } }, // A major
			{ 10,{ 10 ,0 ,2 ,3 ,5 ,7 ,9 } }, // A# major
			{ 11,{ 11 ,1 ,3 ,4 ,6 ,8 ,10 } } // B major
		};

	public:
		static constexpr std::string_view Name = "KeyDetector";
		std::string_view GetStageName() const override { return Name; }

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
				if (it->second < PRUNE_THRESHOLD)
					it = frequencyAccumulator.erase(it);
				else
					++it;
			}

			for (const auto& note : context->Notes)
			{
				const double weight = note.PeakInfo.Score * dt;
				frequencyAccumulator[(int)std::round(note.PeakInfo.Frequency)] += (float)weight;
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

			if (m_Settings.ALGORITHM == "Krumhansl")
				context->PredictedKey = FindKeyKrumhansl(notesHistogram);
			else
				context->PredictedKey = FindKeyScaleMatch(notesHistogram);
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
				},
				"ALGORITHM": {
					"name": "Algorithm",
					"type": "enum",
					"options": ["Scale Match", "Krumhansl"]
				}
			})");
		}

	private:
		KeySignature FindKeyScaleMatch(const std::array<double, 12>& histogram) const
		{
			std::array<double, 12> similarity{};
			FindTotalScaleSimilarity(histogram, similarity);

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

			int relativeMinorIndex = Scales.at(bestKey)[5]; // The 6th note in the major scale is the root of the relative minor
			if (histogram[relativeMinorIndex] > histogram[bestKey])
				return MakeKeySignature(relativeMinorIndex, true);
			return MakeKeySignature(bestKey, false);
		}

		// Krumhansl-Schmuckler: the key whose profile, rotated to its tonic, correlates best with the histogram.
		KeySignature FindKeyKrumhansl(const std::array<double, 12>& histogram) const
		{
			const double histogramMean = std::reduce(histogram.begin(), histogram.end(), 0.0) / 12.0;
			double histogramVariance = 0.0;
			for (double value : histogram)
				histogramVariance += (value - histogramMean) * (value - histogramMean);
			// Every class equally weighted correlates with nothing, so it names no key.
			if (histogramVariance < MIN_HISTOGRAM_VARIANCE)
				return KeySignature{};

			double bestCorrelation = -std::numeric_limits<double>::infinity();
			int bestTonic = 0;
			bool bestIsMinor = false;
			for (int tonic = 0; tonic < 12; tonic++)
			{
				const double major = ProfileCorrelation(histogram, histogramMean, histogramVariance, KrumhanslMajor, tonic);
				if (major > bestCorrelation)
				{
					bestCorrelation = major;
					bestTonic = tonic;
					bestIsMinor = false;
				}

				const double minor = ProfileCorrelation(histogram, histogramMean, histogramVariance, KrumhanslMinor, tonic);
				if (minor > bestCorrelation)
				{
					bestCorrelation = minor;
					bestTonic = tonic;
					bestIsMinor = true;
				}
			}

			return MakeKeySignature(bestTonic, bestIsMinor);
		}

		static double ProfileCorrelation(const std::array<double, 12>& histogram, double histogramMean, double histogramVariance, const std::array<double, 12>& profile, int tonic)
		{
			const double profileMean = std::reduce(profile.begin(), profile.end(), 0.0) / 12.0;
			double covariance = 0.0;
			double profileVariance = 0.0;
			for (int i = 0; i < 12; i++)
			{
				const double profileDeviation = profile[i] - profileMean;
				covariance += (histogram[(tonic + i) % 12] - histogramMean) * profileDeviation;
				profileVariance += profileDeviation * profileDeviation;
			}
			return covariance / std::sqrt(histogramVariance * profileVariance);
		}

		KeySignature MakeKeySignature(int tonicClass, bool isMinor) const
		{
			KeySignature keySig;
			keySig.Tonic = NoteNames.at(tonicClass);
			keySig.TonicClass = tonicClass;
			keySig.ModeName = isMinor ? "Minor" : "Major";
			keySig.Name = keySig.Tonic + " " + keySig.ModeName;
			// A minor key sends its relative major's scale: the same pitch classes as its natural minor.
			keySig.Scale = Scales.at(isMinor ? (tonicClass + 3) % 12 : tonicClass);
			return keySig;
		}

		void FindTotalScaleSimilarity(const std::array<double, 12>& histogram, std::array<double, 12>& outSimilarity) const
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
