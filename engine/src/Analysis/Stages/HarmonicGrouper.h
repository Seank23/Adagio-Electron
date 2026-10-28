#pragma once
#include "../AnalysisStage.h"
#include "../AnalysisPipeline.h"
#include "../AnalysisUtils.h"

#include <algorithm>

namespace Adagio
{
	struct HarmonicGrouperSettings
	{
		int MATCH_CENTS = 25;

		NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(HarmonicGrouperSettings, MATCH_CENTS)
	};

	class HarmonicGrouper : public ConfigurableStage<HarmonicGrouperSettings>
	{
	private:
		static constexpr int MIN_BINS = 2;

	public:
		static constexpr std::string_view Name = "HarmonicGrouper";
		std::string_view GetStageName() const override { return Name; }

		virtual void Execute(AnalysisContext* context) override
		{
			float nyquist = context->Frame.SampleRate / 2.0f;
			float minTolerance = MIN_BINS * context->BinHz;
			float centsRatio = std::pow(2.0f, m_Settings.MATCH_CENTS / 1200.0f) - 1.0f;
			context->HarmonicGroups.assign(context->Partials.size(), {});

			int i = 0;
			for (Partial& candidate : context->Partials)
			{
				std::vector<int> series{ i };
				int k = 2;
				while (k * candidate.Frequency < nyquist)
				{
					int matchIndex = FindMatchIndex(context, k * candidate.Frequency, minTolerance, centsRatio);
					series.push_back(matchIndex);
					k++;
				}
				context->HarmonicGroups[i] = std::move(series);
				i++;
			}
		}

		virtual AnalysisStageType GetType() const override
		{
			return AnalysisStageType::FeatureExtractor;
		}

	protected:
		virtual nlohmann::json BuildSettingsSchema() const override
		{
			return nlohmann::json::parse(R"({
				"MATCH_CENTS": {
					"name": "Match Cents",
					"type": "int",
					"min": 5,
					"max": 50
				}
			})");
		}

	private:
		static int FindMatchIndex(AnalysisContext* context, float targetFreq, float minTolerance, float centsRatio)
		{
			float tolerance = std::max(minTolerance, targetFreq * centsRatio);
			auto closestAboveIt = std::lower_bound(context->Partials.begin(), context->Partials.end(), targetFreq, [](const Partial& p, float freq) { return p.Frequency < freq; });
			int match = -1;
			if (closestAboveIt != context->Partials.begin() && targetFreq - std::prev(closestAboveIt)->Frequency <= tolerance)
			{
				match = (int)(std::prev(closestAboveIt) - context->Partials.begin());
				tolerance = targetFreq - std::prev(closestAboveIt)->Frequency;
			}
			if (closestAboveIt != context->Partials.end() && closestAboveIt->Frequency - targetFreq <= tolerance)
				match = (int)(closestAboveIt - context->Partials.begin());
			return match;
		}
	};
}
