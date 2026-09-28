#pragma once
#include "AnalysisStage.h"
#include "AnalysisPipeline.h"

#include <kfr/dsp.hpp>

namespace Adagio
{
	struct SpectrumFilterProcessorSettings
	{
		std::string ENABLED = "Yes";
		float LOW_CUT_C1 = 55.0f;
		float LOW_CUT_C2 = 70.0f;
		float HIGH_CUT_C1 = 10000.0f;
		float HIGH_CUT_C2 = 20000.0f;

		NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(SpectrumFilterProcessorSettings, ENABLED, LOW_CUT_C1, LOW_CUT_C2, HIGH_CUT_C1, HIGH_CUT_C2)
	};

	class SpectrumFilterProcessor : public ConfigurableStage<SpectrumFilterProcessorSettings>
	{
	public:
		virtual void Execute(AnalysisContext* context) override
		{
			auto& data = context->Magnitudes;
			const size_t frameLength = data.size();

			if (m_Settings.ENABLED == "Yes")
			{
				float lowCutC1 = m_Settings.LOW_CUT_C1;
				float lowCutC2 = m_Settings.LOW_CUT_C2;
				float highCutC1 = m_Settings.HIGH_CUT_C1;
				float highCutC2 = m_Settings.HIGH_CUT_C2;
				kfr::univector<float> filtered(frameLength);
				for (size_t i = 0; i < frameLength; i++)
				{
					float freq = i * context->BinHz;
					if (freq < lowCutC1)
						filtered[i] = 0.0f;
					else if (freq < lowCutC2)
						filtered[i] = data[i] * (freq - lowCutC1) / (lowCutC2 - lowCutC1);
					else if (freq > highCutC2)
						filtered[i] = 0.0f;
					else if (freq > highCutC1)
						filtered[i] = data[i] * (highCutC2 - freq) / (highCutC2 - highCutC1);
					else
						filtered[i] = data[i];
				}
				context->Magnitudes = std::move(filtered);
			}
		}

		virtual AnalysisStageType GetType() const override
		{
			return AnalysisStageType::Processor;
		}

	protected:
		virtual nlohmann::json BuildSettingsSchema() const override
		{
			return nlohmann::json::parse(R"({
				"ENABLED": {
					"name": "Enabled",
					"type": "enum",	
					"options": ["Yes","No"]
				},
				"LOW_CUT_C1": {
					"name": "Low Cut C1",
					"type": "float",	
					"min": 0.0,
					"max": 20000.0
				},
				"LOW_CUT_C2": {
					"name": "Low Cut C2",
					"type": "float",	
					"min": 0.0,
					"max": 20000.0
				},
				"HIGH_CUT_C1": {
					"name": "High Cut C1",
					"type": "float",	
					"min": 0.0,
					"max": 20000.0
				},
				"HIGH_CUT_C2": {
					"name": "High Cut C2",
					"type": "float",	
					"min": 0.0,
					"max": 20000.0
				}	
			})");
		}
	};
}
