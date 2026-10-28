#pragma once
#include "../AnalysisStage.h"
#include "../AnalysisPipeline.h"
#include "../AnalysisUtils.h"

#include <algorithm>

namespace Adagio
{
	struct NoteSelectorSettings
	{
		int HARMONICS = 3;
		float STOP_DB = 30.0f;
		int MAX_NOTES = 8;
		float MIN_FREQ = 50.0f;
		float MAX_FREQ = 4000.0f;

		NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(NoteSelectorSettings, HARMONICS, STOP_DB, MAX_NOTES, MIN_FREQ, MAX_FREQ)
	};

	class NoteSelector : public ConfigurableStage<NoteSelectorSettings>
	{
	private:
		static constexpr int MISSING_LEVEL = -30;
		static constexpr float HELD_CENTS = 50.0f;
		static constexpr int MIN_WHITEN_HALF_LEN = 4;
		static constexpr float WHITEN_HALF_LEN_RATIO = 0.4f;
		static constexpr float SNR_WEIGHT = 0.25f;
		static constexpr float MAGNITUDE_WEIGHT = 0.30f;
		static constexpr float PROMINENCE_WEIGHT = 0.45f;

	public:
		static constexpr std::string_view Name = "NoteSelector";
		std::string_view GetStageName() const override { return Name; }

		virtual void Execute(AnalysisContext* context) override
		{
			const int partialCount = context->Partials.size();
			std::vector<float> residuals(partialCount, 0.0f);
			for (size_t i = 0; i < residuals.size(); i++)
				residuals[i] = context->Partials[i].Magnitude;

			std::vector<int> notes;
			float firstSalience = 0.0f;
			std::vector<bool> rejected(partialCount, false);
			while (notes.size() < m_Settings.MAX_NOTES)
			{
				int bestCandidate = -1;
				float bestRank = 0.0f;
				float maxSalience = 0.0f;
				for (int i = 0; i < partialCount; i++)
				{
					const Partial& p = context->Partials[i];
					// From half the Nyquist frequency up a partial has no harmonic to check, so nothing tells it from another
					// note's partial or a noise peak, and its salience would be its own level.
					if (context->HarmonicGroups[i].size() == 1)
						continue;
					if (rejected[i] || p.Frequency < m_Settings.MIN_FREQ || p.Frequency > m_Settings.MAX_FREQ || residuals[i] < p.Magnitude / 4.0f || residuals[i] < context->GateLevel)
						continue;

					float salience = FindSalience(context, i, residuals);
					maxSalience = std::max(maxSalience, salience);
					// The stop level is eased twice over, so a held note stays until it is clearly gone.
					if (salience < firstSalience * std::powf(10.0f, -m_Settings.STOP_DB / 20.0f))
						continue;
					if (salience > bestRank)
					{
						bestRank = salience;
						bestCandidate = i;
					}
				}
				if (firstSalience == 0.0f)
					firstSalience = maxSalience;

				if (bestCandidate == -1)
					break;
				if (!notes.empty() && !Supported(context, bestCandidate, residuals))
				{
					rejected[bestCandidate] = true;
					continue;
				}
				notes.push_back(bestCandidate);
				UpdateResiduals(context, bestCandidate, residuals);
			}
			std::vector<Peak> selectedNotes;
			const float maxFreq = std::min(m_Settings.MAX_FREQ, context->Frame.SampleRate / 2.0f);
			const int startBin = std::max(1, (int)std::ceil(m_Settings.MIN_FREQ / context->BinHz));
			const int endBin = std::min((int)context->Magnitudes.size() - 2, (int)std::floor(maxFreq / context->BinHz));
			const float floorDb = AnalysisUtils::ToDb(AnalysisUtils::Median(context->Magnitudes, startBin, endBin));
			const float maxInBand = AnalysisUtils::MaxOverRange(context->Magnitudes, startBin, endBin);
			PersistentData& persistent = *context->PersistentData;
			persistent.SelectedFrequencies.clear();
			for (int i : notes)
			{
				selectedNotes.push_back({ context->Partials[i].Frequency, context->Partials[i].Magnitude, Score(context, i, floorDb, maxInBand) });
				persistent.SelectedFrequencies.push_back(context->Partials[i].Frequency);
			}
			persistent.SelectedTimestamp = context->Frame.Timestamp;

			std::sort(selectedNotes.begin(), selectedNotes.end(), [](const Peak& a, const Peak& b)
			{
				return a.Frequency < b.Frequency;
			});
			context->Peaks = std::move(selectedNotes);
		}

		virtual AnalysisStageType GetType() const override
		{
			return AnalysisStageType::FeatureExtractor;
		}

	protected:
		virtual nlohmann::json BuildSettingsSchema() const override
		{
			return nlohmann::json::parse(R"({
				"HARMONICS": {
					"name": "Harmonics",
					"type": "int",
					"min": 1,
					"max": 5
				},
				"STOP_DB": {
					"name": "Stop dB",
					"type": "float",
					"min": 10.0,
					"max": 80.0
				},
				"MAX_NOTES": {
					"name": "Max Notes",
					"type": "int",
					"min": 1,
					"max": 32
				},
				"MIN_FREQ": {
					"name": "Min Frequency",
					"type": "float",
					"min": 20.0,
					"max": 500.0
				},
				"MAX_FREQ": {
					"name": "Max Frequency",
					"type": "float",
					"min": 500.0,
					"max": 12000.0
				}
			})");
		}

	public:
		float FindSalience(AnalysisContext* context, int candidate, const std::vector<float>& residuals)
		{
			float missing = std::max(context->NoiseFloor, residuals[candidate] * std::powf(10.0f, MISSING_LEVEL / 20.0f));
			const std::vector<int>& series = context->HarmonicGroups[candidate];
			std::vector<float> harmonics(std::min(m_Settings.HARMONICS + 1, (int)series.size()));
			float meanResiduals = 0.0f;
			for (int k = 0; k < harmonics.size(); k++)
			{
				harmonics[k] = series[k] >= 0 ? residuals[series[k]] : 0.0f;
				meanResiduals += std::log(std::max(harmonics[k], missing));
			}
			return std::exp(meanResiduals / harmonics.size());
		}

	private:

		bool Supported(AnalysisContext* context, int candidate, const std::vector<float>& residuals)
		{
			float neededLevel = std::max(context->GateLevel, residuals[candidate] * std::pow(10.0f, MISSING_LEVEL / 20.0f));
			for (int i = 1; i <= m_Settings.HARMONICS; i++)
			{
				if (i >= context->HarmonicGroups[candidate].size())
					break;
				if (context->HarmonicGroups[candidate][i] != -1 && residuals[context->HarmonicGroups[candidate][i]] >= neededLevel)
					return true;
			}
			return false;
		}

		void UpdateResiduals(AnalysisContext* context, int candidate, std::vector<float>& residuals)
		{
			const std::vector<int>& series = context->HarmonicGroups[candidate];
			int prevPartial = 0;
			for (int i = 0; i < series.size(); i++)
			{
				if (series[i] == -1)
					continue;
				float toRemove = residuals[series[i]];

				if (i > 0)
				{
					int j = prevPartial;
					int m = -1;
					for (int u = i + 1; u < series.size(); u++)
					{
						if (series[u] != -1)
						{
							m = u;
							break;
						}
					}
					float envelope = m != -1
						? std::lerp(context->Partials[series[j]].Magnitude, context->Partials[series[m]].Magnitude, (float)(i - j) / (float)(m - j))
						: context->Partials[series[j]].Magnitude;
					toRemove = std::min(toRemove, envelope);
				}
				residuals[series[i]] -= toRemove;
				prevPartial = i;
			}
		}

		// PeakExtractor's score, taken at the note's peak bin in Magnitudes.
		float Score(AnalysisContext* context, int note, float floorDb, float maxInBand)
		{
			const kfr::univector<float>& data = context->Magnitudes;

			const int bin = context->Partials[note].BinIndex;
			const int halfWidth = std::max(MIN_WHITEN_HALF_LEN, (int)(bin * WHITEN_HALF_LEN_RATIO));
			const float whitened = data[bin] - AnalysisUtils::Median(data, std::max(bin - halfWidth, 0), std::min(bin + halfWidth, (int)data.size() - 1));
			const float prominence = data[bin] - std::max(data[bin - 1], data[bin + 1]);

			const float normSnr = AnalysisUtils::NormaliseAboveFloor(whitened, floorDb, context->Harmonics);
			const float normMag = maxInBand > 0.0f ? std::min(1.0f, data[bin] / maxInBand) : 0.0f;
			const float normProm = AnalysisUtils::NormaliseAboveFloor(prominence, floorDb, context->Harmonics);
			return SNR_WEIGHT * normSnr + MAGNITUDE_WEIGHT * normMag + PROMINENCE_WEIGHT * normProm;
		}
	};
}
