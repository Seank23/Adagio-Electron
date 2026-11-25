#pragma once
#include "../AnalysisStage.h"
#include "../AnalysisPipeline.h"
#include "../AnalysisUtils.h"

#include <algorithm>

namespace Adagio
{
	struct CQTHarmonicSuppressorSettings
	{
		int HARMONICS = 7;
		int MATCH_CENTS = 30;
		float KEEP_DB = 6.0f;
		float STRENGTH = 1.0f;
		float ROOT_DB = 12.0f;
		std::string SUPPRESS_OCTAVES = "Yes";


		NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(CQTHarmonicSuppressorSettings, HARMONICS, MATCH_CENTS, KEEP_DB, STRENGTH, ROOT_DB, SUPPRESS_OCTAVES)
	};

	class CQTHarmonicSuppressor : public ConfigurableStage<CQTHarmonicSuppressorSettings>
	{
	private:

	public:
		static constexpr std::string_view Name = "CQTHarmonicSuppressor";
		std::string_view GetStageName() const override { return Name; }

		virtual void Execute(AnalysisContext* context) override
		{
			if (m_Settings.HARMONICS == 0 || m_Settings.STRENGTH == 0)
				return;

			const kfr::univector<float>& original = context->Magnitudes;
			if (original.size() < 3)
				return;
			kfr::univector<float> suppressed(original.size());
			std::copy(original.begin(), original.end(), suppressed.begin());

			float floor = AnalysisUtils::Median(original, 0, original.size() - 1);

			float floorMagnitude = floor * std::pow(10.0f, m_Settings.ROOT_DB / 20.0f);
			for (int k = 0; k < original.size() ; k++)
			{
				if (!AnalysisUtils::IsLocalMax(suppressed, k) || suppressed[k] < floorMagnitude)
					continue;
				float freq = context->FreqAxis->BinToHz(AnalysisUtils::ParabolicInterpolation(suppressed, k));

				for (int h = 1; h <= m_Settings.HARMONICS; h++)
				{
					if (m_Settings.SUPPRESS_OCTAVES == "No" && std::log2(h + 1) - std::floor(std::log2(h + 1)) < 0.01f)
						continue;
					int peakBin = NearestPeak(context, suppressed, freq * (h + 1));
					if (peakBin == -1)
						continue;
					float below = LevelNear(context, original, freq * h);
					float above = LevelNear(context, original, freq * (h + 2));
					float envelope = above < 0.0f ? below : AnalysisUtils::GeometricMean({ below, above });
					float credit = envelope * std::pow(10.0f, m_Settings.KEEP_DB / 20.0f);
					float residual = std::sqrt(std::max(0.0f, suppressed[peakBin] * suppressed[peakBin] - credit * credit));
					Attenuate(suppressed, peakBin, residual);
				}
			}
			context->Magnitudes = std::move(suppressed);
		}

		virtual AnalysisStageType GetType() const override
		{
			return AnalysisStageType::Processor;
		}

	protected:
		virtual nlohmann::json BuildSettingsSchema() const override
		{
			return nlohmann::json::parse(R"({
				"HARMONICS": {
					"name": "Number of Harmonics",
					"type": "int",
					"min": 0,
					"max": 11
				},
				"MATCH_CENTS": {
					"name": "Match Tolerance",
					"type": "int",
					"unit": "\u00a2",
					"min": 5,
					"max": 50
				},
				"KEEP_DB": {
					"name": "Keep Threshold",
					"type": "float",
					"unit": "dB",
					"min": 0.0,
					"max": 24.0
				},
				"STRENGTH": {
					"name": "Suppression Strength",
					"type": "float",
					"min": 0.0,
					"max": 1.0
				},
				"ROOT_DB": {
					"name": "Root Threshold",
					"type": "float",
					"unit": "dB",
					"min": 0.0,
					"max": 40.0
				},
				"SUPPRESS_OCTAVES": {
					"name": "Suppress Octaves",
					"type": "enum",
					"options": ["Yes", "No"]
				}
			})");
		}

	private:
		int NearestPeak(AnalysisContext* context, const kfr::univector<float>& spectrum, float targetFreq)
		{
			float bin = context->FreqAxis->HzToFractionalBin(targetFreq);
			if (bin < 0 || bin >= spectrum.size() - 1)
				return -1;

			float lower = context->FreqAxis->HzToFractionalBin(targetFreq * std::pow(2.0f, -m_Settings.MATCH_CENTS / 1200.0f));
			float higher = context->FreqAxis->HzToFractionalBin(targetFreq * std::pow(2.0f, m_Settings.MATCH_CENTS / 1200.0f));
			int best = -1;
			float bestCents = m_Settings.MATCH_CENTS + 1.0f;
			for (int i = (int)std::ceil(lower) - 1; i <= (int)std::floor(higher) + 1; i++)
			{
				if (i < 0 || i >= spectrum.size())
					continue;
				if (!AnalysisUtils::IsLocalMax(spectrum, i))
					continue;
				float cents = std::abs(1200.0f * std::log2(context->FreqAxis->BinToHz(AnalysisUtils::ParabolicInterpolation(spectrum, i)) / targetFreq));
				if (cents <= m_Settings.MATCH_CENTS && (best == -1 || cents < bestCents))
				{
					best = i;
					bestCents = cents;
				}
			}
			return best;
		}

		float LevelNear(AnalysisContext* context, const kfr::univector<float>& spectrum, float freq)
		{
			float bin = context->FreqAxis->HzToFractionalBin(freq);
			if (bin < 0 || bin >= spectrum.size() - 1)
				return -1.0f;

			float lower = context->FreqAxis->HzToFractionalBin(freq * std::pow(2.0f, -m_Settings.MATCH_CENTS / 1200.0f));
			float higher = context->FreqAxis->HzToFractionalBin(freq * std::pow(2.0f, m_Settings.MATCH_CENTS / 1200.0f));
			int roundedBin = (int)std::round(bin);
			int first = std::max(0, std::min((int)std::ceil(lower), roundedBin - 1));
			int last = std::min((int)spectrum.size() - 1, std::max((int)std::floor(higher), roundedBin + 1));
			return *std::max_element(spectrum.begin() + first, spectrum.begin() + last + 1);
		}

		void Attenuate(kfr::univector<float>& spectrum, int localMaxBin, float residual)
		{
			int lower = localMaxBin;
			while (lower > 0 && spectrum[lower - 1] < spectrum[lower])
				lower--;
			int upper = localMaxBin;
			while (upper < (int)spectrum.size() - 1 && spectrum[upper + 1] <= spectrum[upper])
				upper++;

			auto base = [&spectrum, lower, upper](int bin)
				{
					return spectrum[lower] + (spectrum[upper] - spectrum[lower]) * (bin - lower) / (upper - lower);
				};
			float gain = std::clamp((residual - base(localMaxBin)) / (spectrum[localMaxBin] - base(localMaxBin)), 0.0f, 1.0f);
			float gainStrength = 1.0f - m_Settings.STRENGTH * (1.0f - gain);

			for (int i = lower + 1; i < upper; i++)
			{
				if (spectrum[i] > base(i))
					spectrum[i] = base(i) + gainStrength * (spectrum[i] - base(i));
			}
		}
	};
}
