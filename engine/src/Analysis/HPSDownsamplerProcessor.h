#pragma once
#include "AnalysisStage.h"
#include "AnalysisPipeline.h"

#include <kfr/dsp.hpp>

namespace Adagio
{
	struct HPSDownsamplerProcessorSettings
	{
		int HARMONICS = 2;
		int INTERP_FACTOR = 1;
		float MAG_SCALE = 1.0f;
		std::string SQUARE = "No";
		float FLOOR = 1.0f;
		std::string NORMALIZE = "No";

		NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(HPSDownsamplerProcessorSettings, HARMONICS, INTERP_FACTOR, MAG_SCALE, SQUARE, FLOOR, NORMALIZE)
	};

	class HPSDownsamplerProcessor : public ConfigurableStage<HPSDownsamplerProcessorSettings>
	{
	public:
		static constexpr std::string_view Name = "HPSDownsamplerProcessor";
		std::string_view GetStageName() const override { return Name; }

		virtual void Execute(AnalysisContext* context) override
		{
			auto& data = context->Magnitudes;
			const size_t frameLength = data.size();

			const float floor = m_Settings.FLOOR;
			const int harmonics = m_Settings.HARMONICS;
			const float magScale = m_Settings.MAG_SCALE;
			const std::string shouldSquare = m_Settings.SQUARE;
			const std::string normalizeOutput = m_Settings.NORMALIZE;
			const int interpolationFactor = m_Settings.INTERP_FACTOR;

			if (floor > 0.0f)
			{
				for (size_t i = 0; i < frameLength; ++i)
					data[i] += floor;
			}

			std::vector<kfr::univector<float>> spectrums;
			spectrums.push_back(data);

			for (int i = 0; i < harmonics; i++)
			{
				kfr::univector<float> downsampled(frameLength);
				Downsample(downsampled, data, i + 2);
				spectrums.push_back(std::move(downsampled));
			}

			float maxVal = 1.0f;
			if (normalizeOutput == "Before Downsampling")
				maxVal = *std::max_element(data.begin(), data.end());

			kfr::univector<float> productSpectrum(frameLength);
			for (int i = 0; i < frameLength; i++)
			{
				productSpectrum[i] = 1.0f;
				for (int j = 0; j < spectrums.size(); j++)
					productSpectrum[i] *= spectrums[j][i];
				productSpectrum[i] -= floor;
				productSpectrum[i] *= magScale;
				if (shouldSquare == "Yes")
					productSpectrum[i] *= productSpectrum[i];
				if (std::isnan(productSpectrum[i]))
					productSpectrum[i] = 0.0f;
				if (normalizeOutput == "Before Downsampling")
					productSpectrum[i] /= maxVal > 0.0f ? maxVal : 1.0f;
			}
			if (normalizeOutput == "After Downsampling")
			{
				float maxProductVal = *std::max_element(productSpectrum.begin(), productSpectrum.end());
				for (int i = 0; i < productSpectrum.size(); i++)
					productSpectrum[i] /= maxProductVal > 0.0f ? maxProductVal : 1.0f;
			}

			context->Harmonics = harmonics;

			if (interpolationFactor > 1)
			{
				kfr::univector<float> interpolated(productSpectrum.size() * interpolationFactor);
				Interpolate(interpolated, productSpectrum, interpolationFactor);
				context->BinHz /= (float)interpolationFactor;
				context->Magnitudes.resize(interpolated.size());
				context->Magnitudes = std::move(interpolated);
			}
			else
			{
				context->Magnitudes.resize(productSpectrum.size());
				context->Magnitudes = std::move(productSpectrum);
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
				"HARMONICS": {
					"name": "Number of Harmonics",
					"type": "int",	
					"min": 0,
					"max": 5
				},
				"INTERP_FACTOR": {
					"name": "Interpolation Factor",
					"type": "int",	
					"min": 1,
					"max": 5
				},
				"MAG_SCALE": {
					"name": "Magnitude Scale Factor",
					"type": "float",	
					"min": 1.0,
					"max": 5.0
				},
				"SQUARE": {
					"name": "Square Output",
					"type": "enum",	
					"options": ["Yes","No"]
				},
				"FLOOR": {
					"name": "Spectrum Floor",
					"type": "float",	
					"min": 0.0,
					"max": 10.0
				},
				"NORMALIZE": {
					"name": "Normalize Output",
					"type": "enum",	
					"options": ["Before Downsampling", "After Downsampling", "No"]
				}
			})");
		}

	private:
		void Downsample(kfr::univector<float>& outData, const kfr::univector<float> srcData, int sampleFactor) const
		{
			if (sampleFactor > 0)
			{
				int outIndex = 0;
				for (int i = 0; i < srcData.size() - sampleFactor + 1; i += sampleFactor)
				{
					float avgVal = 0.0f;
					for (int j = 0; j < sampleFactor; j++)
						avgVal += srcData[i + j];
					avgVal /= sampleFactor;
					outData[outIndex++] = avgVal;
				}
				for (int i = outIndex; i < outData.size(); i++)
					outData[i] = 1.0f;
			}
		}

		void Interpolate(kfr::univector<float>& outData, const kfr::univector<float> srcData, int interpolationFactor) const
		{
			if (interpolationFactor > 0)
			{
				int outIndex = 0;
				for (int i = 0; i < srcData.size() - 1; i++)
				{

					outData[outIndex++] = srcData[i];
					float delta = srcData[i + 1] - srcData[i];
					for (int j = 1; j < interpolationFactor; j++)
					{
						outData[outIndex++] = srcData[i] + delta * (static_cast<float>(j) / static_cast<float>(interpolationFactor));
					}
				}
			}
		}
	};
}
