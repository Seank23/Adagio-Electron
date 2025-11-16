#pragma once
#include "AnalysisProcessor.h"

#include <kfr/dft.hpp>
// #include <kfr/dft/impl/dft-impl.hpp>
// #include <kfr/dft/impl/fft-impl.hpp>

namespace Adagio
{
	class FFTProcessor : public AnalysisProcessor
	{
	public:
		virtual void* Execute(void* data, void* args) const
		{
			auto dataCast = (kfr::univector<float>*)data;
			const size_t frameLength = dataCast->size();

			kfr::univector<kfr::complex<float>> fftInput;
			fftInput.resize(frameLength);
			
			auto dataPtr = dataCast->begin();
			for (int i = 0; i < frameLength; i++)
				fftInput[i] = kfr::complex<float>{ dataPtr[i], 0.0f };

			auto fftOutputComplex = kfr::dft(fftInput);
			kfr::univector<float>* fftOutputReal = new kfr::univector<float>;
			for (int i = 0; i < fftOutputComplex.size(); i++)
			{
				float real = std::sqrt(std::pow(fftOutputComplex[i].real(), 2.0f) + std::pow(fftOutputComplex[i].imag(), 2.0f));
				fftOutputReal->push_back(real);
			}
			return (void*)fftOutputReal;
		}
	};
}
