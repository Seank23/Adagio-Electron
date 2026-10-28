#pragma once
#include "../AnalysisStage.h"
#include "../AnalysisPipeline.h"
#include "../AnalysisUtils.h"

#include <array>
#include <span>

namespace Adagio
{
	struct PartialExtractorSettings
	{
		float GATE_DB = 4.0f;

		NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(PartialExtractorSettings, GATE_DB)
	};

	class PartialExtractor : public ConfigurableStage<PartialExtractorSettings>
	{
	private:
		static constexpr float START_FREQ = 40.0f;
		static constexpr int MAX_WINDOW_HALF_LEN = 3;
		static constexpr int MEDIAN_WINDOW_HALF_LEN = 12;

	public:
		static constexpr std::string_view Name = "PartialExtractor";
		std::string_view GetStageName() const override { return Name; }

		virtual void Execute(AnalysisContext* context) override
		{
			auto& data = context->Magnitudes;
			const size_t binCount = data.size();

			int startBin = std::max(4, (int)std::ceil(START_FREQ / context->BinHz));
			float noiseFloor = AnalysisUtils::Median(data, startBin, binCount - 1);
			float gate = std::pow(10.0f, m_Settings.GATE_DB / 20.0f);
			std::vector<Partial> partials;
			std::array<float, 2 * MEDIAN_WINDOW_HALF_LEN + 1> medianWindow;

			for (int b = startBin; b < binCount - 1; ++b)
			{
				float maxInWindow = AnalysisUtils::MaxOverRange(data, std::max(b - MAX_WINDOW_HALF_LEN, 0), std::min(b + MAX_WINDOW_HALF_LEN, (int)binCount - 1));
				if (data[b] < maxInWindow)
					continue;

				const int medianStart = std::max(b - MEDIAN_WINDOW_HALF_LEN, 0);
				const int medianEnd = std::min(b + MEDIAN_WINDOW_HALF_LEN, (int)binCount - 1);
				std::copy(data.begin() + medianStart, data.begin() + medianEnd + 1, medianWindow.begin());
				float medianInWindow = AnalysisUtils::MedianBand(std::span(medianWindow).first((size_t)(medianEnd - medianStart + 1)));
				if (data[b] <= gate * medianInWindow)
					continue;

				float parabolicFreq = AnalysisUtils::ParabolicInterpolation(data, b);
				partials.push_back({ b, parabolicFreq * context->BinHz, data[b] });
			}

			context->Partials = std::move(partials);
			context->NoiseFloor = noiseFloor;
			context->GateLevel = gate * noiseFloor;
		}

		virtual AnalysisStageType GetType() const override
		{
			return AnalysisStageType::FeatureExtractor;
		}

	protected:
		virtual nlohmann::json BuildSettingsSchema() const override
		{
			return nlohmann::json::parse(R"({
				"GATE_DB": {
					"name": "Gate Level dB",
					"type": "float",	
					"min": 0.0,
					"max": 40.0
				}
			})");
		}
	};
}
