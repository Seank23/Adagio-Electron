#pragma once
#include "AnalysisStage.h"
#include "AnalysisPipeline.h"

#include <kfr/dsp.hpp>
#include <kfr/dft.hpp>

namespace Adagio
{
	struct FFTProcessorSettings
	{
		std::string WINDOW = "Hamming";

		NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(FFTProcessorSettings, WINDOW)
	};

	class FFTProcessor : public ConfigurableStage<FFTProcessorSettings>
	{
	public:
		static constexpr std::string_view Name = "FFTProcessor";
		std::string_view GetStageName() const override { return Name; }

		virtual void Execute(AnalysisContext* context) override
		{
			auto& data = context->Samples;
			const size_t frameLength = data.size();

			const std::string windowType = m_Settings.WINDOW;

			if (windowType == "Hamming")
				data *= kfr::window_hamming<float>(frameLength);
			else if (windowType == "Hann")
				data *= kfr::window_hann<float>(frameLength);
			else if (windowType == "BlackmannHarris")
				data *= kfr::window_blackman_harris<float>(frameLength);

			kfr::univector<kfr::complex<float>> fftInput;
			fftInput.resize(frameLength);

			auto dataPtr = data.begin();
			for (int i = 0; i < frameLength; i++)
				fftInput[i] = kfr::complex<float>{ dataPtr[i], 0.0f };

			auto fftOutputComplex = kfr::dft(fftInput);
			kfr::univector<float> fftOutputMagnitude;

			for (int i = 0; i < fftOutputComplex.size(); i++)
				fftOutputMagnitude.push_back(std::abs(fftOutputComplex[i]));

			context->BinHz = context->Frame.SampleRate / (float)fftOutputMagnitude.size();
			fftOutputMagnitude = fftOutputMagnitude.truncate(fftOutputMagnitude.size() / 2);
			context->Spectrum = std::move(fftOutputComplex);
			context->Magnitudes = std::move(fftOutputMagnitude);
		}

		virtual AnalysisStageType GetType() const override
		{
			return AnalysisStageType::Processor;
		}

	protected:
		virtual nlohmann::json BuildSettingsSchema() const override
		{
			return nlohmann::json::parse(R"({
				"WINDOW": {
					"name": "Window Function",
					"type": "enum",	
					"options": ["Rectangle","Hamming","Hann","BlackmannHarris"]
				}
			})");
		}
	};
}
