#pragma once
#include "AnalysisStage.h"
#include "AnalysisPipeline.h"

#include <kfr/dft.hpp>

namespace Adagio
{
	class FFTProcessor : public AnalysisStage
	{
	public:
		virtual void Execute(AnalysisContext* context) const override
		{
			auto& data = context->Windowed;
			const size_t frameLength = data.size();

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
	};
}
