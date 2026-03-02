#pragma once
#include "AnalysisStage.h"
#include "AnalysisPipeline.h"

#include <kfr/dsp.hpp>
#include <kfr/dft.hpp>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <limits>

namespace Adagio
{
	class PeakExtractor : public AnalysisStage
	{
	public:
		virtual void Execute(AnalysisContext* context) const override
		{
			auto& data = context->Magnitudes;
			const size_t bins = data.size();
			if (bins < 3)
			{
				context->Peaks.clear();
				return;
			}

			nlohmann::json settings = context->Settings;

			const float minFreq = settings.value("MIN_FREQ", 50.0f);
			const float maxFreqSetting = settings.value("MAX_FREQ", 5000.0f);
			const float minSemitoneDistance = settings.value("MIN_SEMITONE_DISTANCE", 0.25f);
			const int maxPeaks = settings.value("MAX_PEAKS", 16);
			const bool useDb = settings.value("USE_DB", false);
			const float minProminence = settings.value("MIN_PROMINENCE", useDb ? 3.0f : 0.1f);
			const float minSnr = settings.value("MIN_SNR", useDb ? 8.0f : 0.5f);

			const float sampleRate = static_cast<float>(context->Frame.SampleRate);
			const float binToFreq = sampleRate / static_cast<float>(bins);
			const float nyquist = sampleRate * 0.5f;
			const float maxFreq = std::min(maxFreqSetting, nyquist);

			const size_t startBin = static_cast<size_t>(std::max(1.0f, std::ceil(minFreq / binToFreq)));
			const size_t endBin = static_cast<size_t>(std::min(static_cast<float>(bins - 2), std::floor(maxFreq / binToFreq)));

			if (startBin >= endBin)
			{
				context->Peaks.clear();
				return;
			}

			// Build the working spectrum in the chosen domain.
			std::vector<float> spectrum(bins);
			if (useDb)
			{
				for (size_t i = 0; i < bins; ++i)
					spectrum[i] = ToDb(data[i]);
			}
			else
			{
				for (size_t i = 0; i < bins; ++i)
					spectrum[i] = data[i];
			}

			// Spectral whitening + peak detection in a single pass.
			// Uses a reusable scratch buffer with nth_element for the
			// local median instead of allocating a new vector per bin.
			const size_t maxWindow = std::max<size_t>(9, static_cast<size_t>(endBin * 0.8f) + 2);
			std::vector<float> scratch(maxWindow);

			struct PeakCandidate
			{
				size_t bin;
				float freq;
				float mag;
				float specVal;
				float prominence;
				float whitened;
				float score;
			};

			std::vector<std::pair<size_t, float>> localMedianArray;
			std::vector<PeakCandidate> candidates;
			for (size_t i = startBin; i <= endBin; ++i)
			{
				// Fast local-maximum pre-check before computing the median
				if (!(spectrum[i] > spectrum[i - 1] && spectrum[i] >= spectrum[i + 1]))
					continue;

				// Prominence: how far this peak rises above its immediate neighbours
				const float prominence = spectrum[i] - std::max(spectrum[i - 1], spectrum[i + 1]);
				if (prominence < 0.05) // Global prominence rejection
					continue;

				// Compute whitened value: local median subtraction using a
				// reusable scratch buffer and O(n) nth_element.
				const size_t halfWidth = std::max<size_t>(4, static_cast<size_t>(i * 0.4f));
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
				const float whitened = spectrum[i] - localMedian;
				localMedianArray.push_back({ i, localMedian });

				// SNR: whitened value above local median envelope
				if (whitened < minSnr)
					continue;

				// Local prominence rejection
				if (prominence < minProminence * localMedian)
					continue;

				const float interpolatedBin = ParabolicInterpolation(spectrum, static_cast<int>(i));
				const float freq = interpolatedBin * binToFreq;
				if (freq < minFreq || freq > maxFreq)
					continue;

				candidates.push_back({ i, freq, data[i], spectrum[i], prominence, whitened, 0.0f });
			}

			if (candidates.empty())
			{
				context->Peaks.clear();
				return;
			}

			// Compute a composite score that blends local SNR (whitened),
			// absolute value in the working domain, and prominence.
			// Each component is normalised to [0,1] within this frame's
			// candidates so that no single factor dominates, then weighted.
			float maxWhitened = candidates[0].whitened;
			float minWhitened = candidates[0].whitened;
			float maxSpec = candidates[0].specVal;
			float minSpec = candidates[0].specVal;
			float maxProm = candidates[0].prominence;
			float minProm = candidates[0].prominence;
			for (const auto& c : candidates)
			{
				maxWhitened = std::max(maxWhitened, c.whitened);
				minWhitened = std::min(minWhitened, c.whitened);
				maxSpec = std::max(maxSpec, c.specVal);
				minSpec = std::min(minSpec, c.specVal);
				maxProm = std::max(maxProm, c.prominence);
				minProm = std::min(minProm, c.prominence);
			}
			const float whiteRange = (maxWhitened - minWhitened > 1e-6f) ? (maxWhitened - minWhitened) : 1.0f;
			const float specRange = (maxSpec - minSpec > 1e-6f) ? (maxSpec - minSpec) : 1.0f;
			const float promRange = (maxProm - minProm > 1e-6f) ? (maxProm - minProm) : 1.0f;

			constexpr float wSnr = 0.45f;
			constexpr float wMag = 0.30f;
			constexpr float wProm = 0.25f;
			for (auto& c : candidates)
			{
				const float normSnr = (c.whitened - minWhitened) / whiteRange;
				const float normMag = (c.specVal - minSpec) / specRange;
				const float normProm = (c.prominence - minProm) / promRange;
				c.score = wSnr * normSnr + wMag * normMag + wProm * normProm;
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

			std::vector<std::pair<float, float>> peaks(selected.size());
			for (size_t i = 0; i < selected.size(); ++i)
				peaks[i] = { selected[i].freq, selected[i].mag };

			context->Peaks = std::move(peaks);
			context->LocalMedian = std::move(localMedianArray);
		}

		virtual AnalysisStageType GetType() const override
		{
			return AnalysisStageType::FeatureExtractor;
		}

		virtual nlohmann::json GetSettings() const override
		{
			return nlohmann::json::parse(R"json({
				"USE_DB": {
					"name": "Use dB Scale",
					"type": "bool",
					"default": false
				},
				"MIN_FREQ": {
					"name": "Minimum Frequency (Hz)",
					"type": "float",
					"min": 20.0,
					"max": 500.0,
					"default": 50.0
				},
				"MAX_FREQ": {
					"name": "Maximum Frequency (Hz)",
					"type": "float",
					"min": 500.0,
					"max": 12000.0,
					"default": 5000.0
				},
				"MIN_PROMINENCE": {
					"name": "Min Peak Prominence",
					"type": "float",
					"min": 0.001,
					"max": 20.0,
					"default": 1.5
				},
				"MIN_SNR": {
					"name": "Min SNR Above Local Floor",
					"type": "float",
					"min": 0.001,
					"max": 30.0,
					"default": 1.0
				},
				"MIN_SEMITONE_DISTANCE": {
					"name": "Min Peak Distance (semitones)",
					"type": "float",
					"min": 0.25,
					"max": 3.0,
					"default": 0.5
				},
				"MAX_PEAKS": {
					"name": "Max Peaks",
					"type": "int",
					"min": 1,
					"max": 64,
					"default": 16
				}
			})json");
		}

	private:
		float ToDb(float value) const
		{
			constexpr float epsilon = 1e-12f;
			return 20.0f * std::log10(std::max(value, epsilon));
		}

		float ParabolicInterpolation(const std::vector<float>& data, int k) const
		{
			if (k <= 0 || k >= static_cast<int>(data.size()) - 1)
				return static_cast<float>(k);

			const float alpha = data[k - 1];
			const float beta = data[k];
			const float gamma = data[k + 1];

			const float denominator = alpha - 2.0f * beta + gamma;
			if (std::abs(denominator) < 1e-6f)
				return static_cast<float>(k);

			const float delta = 0.5f * (alpha - gamma) / denominator;
			return static_cast<float>(k) + delta;
		}
	};
}
