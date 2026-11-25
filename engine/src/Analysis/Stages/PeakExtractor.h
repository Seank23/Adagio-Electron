#pragma once
#include "../AnalysisStage.h"
#include "../AnalysisPipeline.h"
#include "../AnalysisUtils.h"

#include <kfr/dsp.hpp>
#include <kfr/dft.hpp>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <limits>

namespace Adagio
{
	struct PeakExtractorSettings
	{
		float MIN_FREQ = 50.0f;
		float MAX_FREQ = 4000.0f;
		float MIN_PROMINENCE = 1.5f;
		float MIN_SNR_DB = 6.0f;
		float MIN_SEMITONE_DISTANCE = 0.5f;
		int MAX_PEAKS = 16;
		float SCORE_THRESHOLD = 0.25f;
		std::string USE_INTERP_BINS = "Yes";

		NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(PeakExtractorSettings, MIN_FREQ, MAX_FREQ, MIN_PROMINENCE, MIN_SNR_DB, MIN_SEMITONE_DISTANCE, MAX_PEAKS, SCORE_THRESHOLD, USE_INTERP_BINS)
	};

	class PeakExtractor : public ConfigurableStage<PeakExtractorSettings>
	{
	private:
		static constexpr float WHITENING_SPAN = 0.4f;

	public:
		static constexpr std::string_view Name = "PeakExtractor";
		std::string_view GetStageName() const override { return Name; }

		virtual void Execute(AnalysisContext* context) override
		{
			auto& data = context->Magnitudes;
			const size_t bins = data.size();
			if (bins < 3)
			{
				context->Peaks.clear();
				return;
			}

			const float minFreq = m_Settings.MIN_FREQ;
			const float maxFreqSetting = m_Settings.MAX_FREQ;
			const float minSemitoneDistance = m_Settings.MIN_SEMITONE_DISTANCE;
			const int maxPeaks = m_Settings.MAX_PEAKS;
			const float minProminence = m_Settings.MIN_PROMINENCE;
			const float minSnrDb = m_Settings.MIN_SNR_DB;
			const float scoreThreshold = m_Settings.SCORE_THRESHOLD;
			const std::string useInterpolatedBins = m_Settings.USE_INTERP_BINS;

			const float sampleRate = static_cast<float>(context->Frame.SampleRate);
			const FrequencyAxis& axis = *context->FreqAxis;
			const bool isLogAxis = axis.IsLog();
			const float nyquist = sampleRate * 0.5f;
			const float maxFreq = std::min(maxFreqSetting, nyquist);

			const size_t startBin = (size_t)(std::max(1.0f, std::ceil(axis.HzToFractionalBin(minFreq))));
			const size_t endBin = (size_t)(std::min(static_cast<float>(bins - 2), std::floor(axis.HzToFractionalBin(maxFreq))));

			if (startBin >= endBin)
			{
				context->Peaks.clear();
				return;
			}

			// Build the working spectrum in the chosen domain.
			std::vector<float> spectrum(bins);
			for (size_t i = 0; i < bins; ++i)
				spectrum[i] = data[i];

			// Spectral whitening + peak detection in a single pass.
			// Uses a reusable scratch buffer with nth_element for the
			// local median instead of allocating a new vector per bin.
			// The median window spans WHITENING_SPAN of a bin's frequency either side: 0.4 * i bins on a linear axis.
			auto halfWidthAt = [&axis](size_t bin)
			{
				const float width = axis.HzToFractionalBin(axis.BinToHz((float)bin) * (1.0f + WHITENING_SPAN)) - (float)bin;
				return std::max<size_t>(4, (size_t)(width + 1e-3f));
			};
			const size_t maxWindow = std::max<size_t>(9, 2 * halfWidthAt(endBin) + 2);
			std::vector<float> scratch(maxWindow);

			struct PeakCandidate
			{
				size_t bin;
				float freq;
				float mag;
				float specVal;
				float prominence;
				float floorDb;
				float score;
			};

			std::vector<PeakCandidate> candidates;
			for (size_t i = startBin; i <= endBin; ++i)
			{
				// Fast local-maximum pre-check before computing the median
				if (!(spectrum[i] > spectrum[i - 1] && spectrum[i] >= spectrum[i + 1]))
					continue;

				const size_t halfWidth = halfWidthAt(i);

				float bin = (float)i;
				float height = spectrum[i];
				if (useInterpolatedBins == "Yes" && isLogAxis)
				{
					const InterpolatedPeak top = AnalysisUtils::LogParabolicPeak(spectrum, (int)i);
					bin = top.Bin;
					height = top.Magnitude;
				}
				else if (useInterpolatedBins == "Yes")
					bin = AnalysisUtils::ParabolicInterpolation(spectrum, (int)i);

				// Prominence: how far this peak rises above its immediate neighbours. On a log axis they sit on its shoulders, higher
				// the further the note is from a bin, so there it is measured down to the valleys instead.
				const float prominence = isLogAxis
					? height - AnalysisUtils::HigherValley(spectrum, i, halfWidth)
					: spectrum[i] - std::max(spectrum[i - 1], spectrum[i + 1]);
				if (prominence <= 0.0f)
					continue;

				// The local floor: the median over the whitening window, using a
				// reusable scratch buffer and O(n) nth_element.
				const size_t lo = (i > halfWidth) ? i - halfWidth : 0;
				const size_t hi = std::min(bins - 1, i + halfWidth);
				const size_t windowLen = hi - lo + 1;

				std::copy(spectrum.data() + lo, spectrum.data() + hi + 1, scratch.data());
				const size_t mid = windowLen / 2;
				std::nth_element(scratch.data(), scratch.data() + mid, scratch.data() + windowLen);
				float localMedian = scratch[mid];
				if ((windowLen & 1) == 0)
				{
					float lower = *std::max_element(scratch.data(), scratch.data() + mid);
					localMedian = (lower + localMedian) * 0.5f;
				}
				// Both gates are ratios to the local floor, so neither moves with the recording's level, the frame length or the
				// transform's scale.
				const float floorDb = AnalysisUtils::ToDb(localMedian);
				const float snrDb = (AnalysisUtils::ToDb(height) - floorDb) / (float)(context->Harmonics + 1);
				if (snrDb < minSnrDb)
					continue;

				// Local prominence rejection
				if (prominence < minProminence * localMedian)
					continue;

				const float freq = axis.BinToHz(bin);
				if (freq < minFreq || freq > maxFreq)
					continue;

				candidates.push_back({ i, freq, height, height, prominence, floorDb, 0.0f });
			}

			if (candidates.empty())
			{
				context->Peaks.clear();
				return;
			}

			// Score each candidate against its own local floor and the frame's maximum, not against the candidate set, so Score
			// stays comparable between frames for the key and chord histograms.
			float maxSpec = spectrum[startBin];
			for (size_t i = startBin; i <= endBin; ++i)
				maxSpec = std::max(maxSpec, spectrum[i]);
			for (const PeakCandidate& c : candidates)
				maxSpec = std::max(maxSpec, c.specVal);

			constexpr float wSnr = 0.25f;
			constexpr float wMag = 0.30f;
			constexpr float wProm = 0.45f;
			for (auto& c : candidates)
			{
				const float normSnr = AnalysisUtils::NormaliseAboveFloor(c.specVal, c.floorDb, context->Harmonics);
				const float normMag = (maxSpec > 0.0f) ? std::min(1.0f, c.specVal / maxSpec) : 0.0f;
				const float normProm = AnalysisUtils::NormaliseAboveFloor(c.prominence, c.floorDb, context->Harmonics);
				c.score = wSnr * normSnr + wMag * normMag + wProm * normProm;
			}

			for (size_t i = 0; i < candidates.size();)
			{
				if (candidates[i].score < scoreThreshold)
					candidates.erase(candidates.begin() + i);
				else
					i++;
			}

			// Sort by composite score (highest first).
			std::sort(candidates.begin(), candidates.end(), [](const PeakCandidate& a, const PeakCandidate& b)
			{
				return a.score > b.score;
			});

			std::vector<PeakCandidate> selected;
			const size_t limit = static_cast<size_t>(std::max(0, maxPeaks));
			selected.reserve(limit);

			for (const auto& cand : candidates)
			{
				if (selected.size() >= limit)
					break;

				bool tooClose = false;
				for (const auto& ref : selected)
				{
					const float semitones = std::abs(12.0f * std::log2(cand.freq / ref.freq));
					if (semitones < minSemitoneDistance)
					{
						tooClose = true;
						break;
					}
				}

				if (!tooClose)
					selected.push_back(cand);
			}

			std::sort(selected.begin(), selected.end(), [](const PeakCandidate& a, const PeakCandidate& b)
			{
				return a.freq < b.freq;
			});

			std::vector<Peak> peaks(selected.size());
			for (size_t i = 0; i < selected.size(); ++i)
				peaks[i] = { selected[i].freq, selected[i].mag, selected[i].score };

			context->Peaks = std::move(peaks);
		}

		virtual AnalysisStageType GetType() const override
		{
			return AnalysisStageType::FeatureExtractor;
		}

	protected:
		virtual nlohmann::json BuildSettingsSchema() const override
		{
			return nlohmann::json::parse(R"json({
				"MIN_FREQ": {
					"name": "Minimum Frequency",
					"type": "float",
					"unit": "Hz",
					"min": 20.0,
					"max": 500.0
				},
				"MAX_FREQ": {
					"name": "Maximum Frequency",
					"type": "float",
					"unit": "Hz",
					"min": 500.0,
					"max": 12000.0
				},
				"MIN_PROMINENCE": {
					"name": "Min Peak Prominence",
					"type": "float",
					"min": 0.001,
					"max": 20.0
				},
				"MIN_SNR_DB": {
					"name": "Min SNR Above Local Floor",
					"type": "float",
					"unit": "dB",
					"min": 0.0,
					"max": 40.0
				},
				"MIN_SEMITONE_DISTANCE": {
					"name": "Min Peak Distance",
					"type": "float",
					"unit": "st",
					"min": 0.25,
					"max": 3.0
				},
				"MAX_PEAKS": {
					"name": "Max Peaks",
					"type": "int",
					"min": 1,
					"max": 64
				},
				"SCORE_THRESHOLD": {
					"name": "Score Threshold",
					"type": "float",
					"min": 0.0,
					"max": 1.0
				},
				"USE_INTERP_BINS": {
					"name": "Use Interpolated Bins",
					"type": "enum",	
					"options": ["Yes","No"]
				}
			})json");
		}
	};
}
