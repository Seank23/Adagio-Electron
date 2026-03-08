#pragma once
#include "AnalysisStage.h"
#include "AnalysisPipeline.h"

#include <kfr/dsp.hpp>

namespace Adagio
{
	class SpectrumFilterProcessor : public AnalysisStage
	{
	public:
		virtual void Execute(AnalysisContext* context) override
		{
			AnalysisStage::Execute(context);
			auto& data = context->Magnitudes;
			const size_t frameLength = data.size();
			nlohmann::json settings = context->Settings;

			const std::string enabled = GetSetting<std::string>(settings, "ENABLED");

			if (enabled == "Yes")
			{
				float lowCutC1 = GetSetting<float>(settings, "LOW_CUT_C1");
				float lowCutC2 = GetSetting<float>(settings, "LOW_CUT_C2");
				float highCutC1 = GetSetting<float>(settings, "HIGH_CUT_C1");
				float highCutC2 = GetSetting<float>(settings, "HIGH_CUT_C2");
				kfr::univector<float> filtered(frameLength);
				for (size_t i = 0; i < frameLength; i++)
				{
					float freq = (i * context->Frame.SampleRate) / frameLength;
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

		virtual nlohmann::json GetSettings() const override
		{
			return nlohmann::json::parse(R"({
				"ENABLED": {
					"name": "Enabled",
					"type": "enum",	
					"options": ["Yes","No"],
					"default": "Yes"
				},
				"LOW_CUT_C1": {
					"name": "Low Cut C1",
					"type": "float",	
					"min": 0.0,
					"max": 20000.0,
					"default": 100.0
				},
				"LOW_CUT_C2": {
					"name": "Low Cut C2",
					"type": "float",	
					"min": 0.0,
					"max": 20000.0,
					"default": 200.0
				},
				"HIGH_CUT_C1": {
					"name": "High Cut C1",
					"type": "float",	
					"min": 0.0,
					"max": 20000.0,
					"default": 10000.0
				},
				"HIGH_CUT_C2": {
					"name": "High Cut C2",
					"type": "float",	
					"min": 0.0,
					"max": 20000.0,
					"default": 20000.0
				}	
			})");
		}
	};
}
