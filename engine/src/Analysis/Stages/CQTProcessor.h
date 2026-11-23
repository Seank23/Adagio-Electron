#pragma once
#include "../AnalysisStage.h"
#include "../AnalysisPipeline.h"
#include "../FrequencyAxis.h"

#include <kfr/dsp.hpp>
#include <kfr/dft.hpp>
#include <numbers>
#include <algorithm>
#include <cmath>

namespace Adagio
{
	struct CQTProcessorSettings
	{
		std::string WINDOW = "Hamming";
		float MIN_FREQ = 55.0f;
		float MAX_FREQ = 4000.0f;
		std::string BINS_PER_OCTAVE = "36";
		float FILTER_SCALE = 1.0f;
		float HIGH_FREQ_SMOOTHING = 0.25f;

		bool operator==(const CQTProcessorSettings& other) const
		{
			return WINDOW == other.WINDOW &&
				MIN_FREQ == other.MIN_FREQ &&
				MAX_FREQ == other.MAX_FREQ &&
				BINS_PER_OCTAVE == other.BINS_PER_OCTAVE &&
				FILTER_SCALE == other.FILTER_SCALE &&
				HIGH_FREQ_SMOOTHING == other.HIGH_FREQ_SMOOTHING;
		}

		NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(CQTProcessorSettings, WINDOW, MIN_FREQ, MAX_FREQ, BINS_PER_OCTAVE, FILTER_SCALE, HIGH_FREQ_SMOOTHING)
	};

	struct KernelRow
	{
		kfr::univector<kfr::complex<float>> Values;
		int StartBin = 0;
		float CenterFreq = 0.0f;
		float WindowSeconds = 0.0f;
	};

	class CQTProcessor : public ConfigurableStage<CQTProcessorSettings>
	{
	private:
		static constexpr float SPARSITY_THRESHOLD = 0.005f;

		std::vector<KernelRow> m_Kernels;
		CQTProcessorSettings m_CachedSettings;
		float m_CachedSampleRate = 0.0f;
		int m_CachedFrameLength = 0;
		float m_WindowGain = 0.5f;

	public:
		static constexpr std::string_view Name = "CQTProcessor";
		std::string_view GetStageName() const override { return Name; }

		virtual void Execute(AnalysisContext* context) override
		{
			auto& data = context->Samples;
			const size_t frameLength = data.size();
			const float sampleRate = context->Frame.SampleRate;

			bool settingsChanged = m_CachedSettings != m_Settings;
			m_CachedSettings = m_Settings;

			if (sampleRate != m_CachedSampleRate || frameLength != m_CachedFrameLength || settingsChanged)
				BuildKernel(sampleRate, frameLength);

			kfr::univector<kfr::complex<float>> fftInput;
			fftInput.resize(frameLength);

			auto dataPtr = data.begin();
			for (int i = 0; i < frameLength; i++)
				fftInput[i] = kfr::complex<float>{ dataPtr[i], 0.0f };

			auto X = kfr::dft(fftInput);

			float gain = GetWindowGain(frameLength);
			kfr::univector<float> cqtMagnitudes(m_Kernels.size());
			for (size_t k = 0; k < m_Kernels.size(); k++)
			{
				const KernelRow& row = m_Kernels[k];
				kfr::complex<float> cq{ 0.0f, 0.0f };

				for (size_t n = 0; n < row.Values.size(); n++)
					cq += X[row.StartBin + n] * row.Values[n];

				cqtMagnitudes[k] = std::abs(cq) * m_CachedFrameLength * gain / m_WindowGain;
			}
			context->Magnitudes = std::move(cqtMagnitudes);

			const float smoothing = std::clamp(m_Settings.HIGH_FREQ_SMOOTHING, 0.0f, 1.0f);
			context->BinSmoothingSeconds.clear();
			if (smoothing > 0.0f && !m_Kernels.empty())
			{
				const float longestWindow = m_Kernels.front().WindowSeconds;
				context->BinSmoothingSeconds.reserve(m_Kernels.size());
				for (const KernelRow& row : m_Kernels)
					context->BinSmoothingSeconds.push_back(smoothing * (longestWindow - row.WindowSeconds));
			}

			const float firstFreq = m_Kernels.empty() ? m_Settings.MIN_FREQ : m_Kernels.front().CenterFreq;
			context->FreqAxis = std::make_unique<LogFrequencyAxis>(firstFreq, (int)std::clamp(std::stof(m_Settings.BINS_PER_OCTAVE), 12.0f, 48.0f));
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
				},
				"MIN_FREQ": {
					"name": "Minimum Frequency",
					"type": "float",
					"unit": "Hz",
					"min": 20.0,
					"max": 500.0
				},
				"MAX_FREQ": {
					"name": "Maximum Frequency",
					"type": "float",
					"unit": "Hz",
					"min": 500.0,
					"max": 8000.0
				},
				"BINS_PER_OCTAVE": {
					"name": "Bins per Octave",
					"type": "enum",	
					"options": ["12","24","36","48"]
				},
				"FILTER_SCALE": {
					"name": "Filter Scale",
					"type": "float",
					"min": 0.25,
					"max": 1.0
				},
				"HIGH_FREQ_SMOOTHING": {
					"name": "High-Frequency Smoothing",
					"type": "float",
					"min": 0.0,
					"max": 1.0
				}
			})");
		}

	private:
		void BuildKernel(float sampleRate, int frameLength)
		{
			m_Kernels.clear();
			m_CachedSampleRate = sampleRate;
			m_CachedFrameLength = frameLength;

			const float binsPerOctave = std::clamp(std::stof(m_Settings.BINS_PER_OCTAVE), 12.0f, 48.0f);
			const float minFreq = std::max(20.0f, m_Settings.MIN_FREQ);
			const float maxFreq = std::min(m_Settings.MAX_FREQ, sampleRate * 0.5f / (float)std::pow(2.0, 0.5 / binsPerOctave));

			if (maxFreq <= minFreq)
				return;

			const int numBins = (int)std::floor(binsPerOctave * std::log2(maxFreq / minFreq)) + 1;
			const double Q = 1.0 / (std::pow(2.0, 1.0 / (double)binsPerOctave) - 1.0);
			const double centre = frameLength * 0.5f;

			const float scale = std::clamp(m_Settings.FILTER_SCALE, 0.25f, 1.0f);
			kfr::univector<kfr::complex<float>> kernel(frameLength);
			for (int k = 0; k < numBins; k++)
			{
				const double f_k = minFreq * std::pow(2.0f, k / binsPerOctave);
				const double N_k = std::ceil(scale * Q * sampleRate / f_k);
				if (N_k > frameLength)
					continue;

				std::fill(kernel.begin(), kernel.end(), kfr::complex<float>{ 0.0f, 0.0f });
				const kfr::univector<float> window = GetWindowFunction((int)N_k);
				const int startBin = (frameLength - (int)N_k) / 2;
				const double omega = 2.0 * std::numbers::pi * f_k / sampleRate;
				const float norm = 1.0f / N_k;

				for (int n = 0; n < N_k; n++)
				{
					const double phase = omega * ((double)(startBin + n) - centre);
					kernel[startBin + n] = std::polar(norm * window[n], (float)phase);
				}
				if (m_Kernels.empty())
					m_WindowGain = kfr::sum(window) / N_k;

				kfr::univector<kfr::complex<float>> fftKernel = kfr::dft(kernel);
				
				float peak = 0.0f;
				for (int j = 0; j < (int)centre; j++)
					peak = std::max(peak, std::abs(fftKernel[j]));

				const float threshold = peak * SPARSITY_THRESHOLD;
				int first = -1, last = -1;
				for (int j = 0; j < (int)centre; j++)
				{
					if (std::abs(fftKernel[j]) >= threshold)
					{
						if (first == -1)
							first = j;
						last = j;
					}
				}

				KernelRow row;
				row.StartBin = first;
				row.CenterFreq = f_k;
				row.WindowSeconds = (float)(N_k / sampleRate);
				row.Values.reserve(last - first + 1);
				const float scale = 1.0f / frameLength;
				for (int j = first; j <= last; j++)
					row.Values.push_back(scale * std::conj(fftKernel[j]));

				m_Kernels.push_back(std::move(row));
			}
		}

		kfr::univector<float>& GetWindowFunction(int length)
		{
			static kfr::univector<float> window;
			window.resize(length);
			if (m_Settings.WINDOW == "Hamming")
				window = kfr::window_hamming<float>(length);
			else if (m_Settings.WINDOW == "Hann")
				window = kfr::window_hann<float>(length);
			else if (m_Settings.WINDOW == "BlackmannHarris")
				window = kfr::window_blackman_harris<float>(length);
			else
				std::fill(window.begin(), window.end(), 1.0f);
			return window;
		}

		float GetWindowGain(int length)
		{
			const kfr::univector<float> window = GetWindowFunction(length);
			return kfr::sum(window) / length;
		}
	};
}
