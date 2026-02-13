#pragma once
#include "AnalysisStage.h"
#include "AnalysisPipeline.h"

#include <kfr/dsp.hpp>
#include <kfr/dft.hpp>

namespace Adagio
{
	class FFTProcessor : public AnalysisStage
	{
	public:
		virtual void Execute(AnalysisContext* context) const override
		{
			auto& data = context->Samples;
			const size_t frameLength = data.size();
			nlohmann::json settings = context->Settings;

			if (settings.contains("WINDOW"))
			{
				std::string windowType = settings["WINDOW"].get<std::string>();
				if (windowType == "Hamming")
					data *= kfr::window_hamming<float>(frameLength);
				else if (windowType == "Hann")
					data *= kfr::window_hann<float>(frameLength);
				else if (windowType == "BlackmannHarris")
					data *= kfr::window_blackman_harris<float>(frameLength);
			}
			else
			{
				// Default to Hamming window
				data *= kfr::window_hamming<float>(frameLength);
			}

			kfr::univector<kfr::complex<float>> fftInput;
			fftInput.resize(frameLength);
			
			auto dataPtr = data.begin();
			for (int i = 0; i < frameLength; i++)
				fftInput[i] = kfr::complex<float>{ dataPtr[i], 0.0f };

			auto fftOutputComplex = kfr::dft(fftInput);
			kfr::univector<float> fftOutputReal;
			for (int i = 0; i < fftOutputComplex.size(); i++)
			{
				float real = fftOutputComplex[i].real();
				fftOutputReal.push_back(real);
			}
			fftOutputReal = fftOutputReal.truncate(fftOutputReal.size() / 2);
			context->Spectrum = std::move(fftOutputComplex);
			context->Magnitudes = std::move(fftOutputReal);
		}

		virtual AnalysisStageType GetType() const override
		{
			return AnalysisStageType::Processor;
		}

		virtual nlohmann::json GetSettings() const override
		{
			return nlohmann::json::parse(R"({
				"WINDOW": {
					"name": "Window Function",
					"type": "enum",	
					"options": ["Rectangle","Hamming","Hann","BlackmannHarris"],
					"default": "Hamming"
				}
			})");
		}
	};
}
