#include "kfr/base.hpp"
#include "AnalysisService.h"
#include "AnalysisPipeline.h"
#include "../Core/AudioDecoder.h"
#include "FFTProcessor.h"
#include "../Core/MessageQueue.h"
#include "../IO/AudioData.h"

#include <nlohmann/json.hpp>
#include <iostream>
#include <chrono>
#include <kfr/dsp.hpp>

namespace Adagio
{
	AnalysisService::AnalysisService()
		: m_Running(false), m_IntervalMs(3), m_RollingAvgCount(8), m_AnalysisBuffer(nullptr)
	{
	}

	AnalysisService::~AnalysisService()
	{
	}

	void AnalysisService::Init(std::shared_ptr<AudioDecoder> decoder, AnalysisParams params)
	{
		m_Decoder = decoder;
		m_Params = params;
		m_AudioSource = m_Decoder->GetAudioSource();

		kfr::univector<float> preprocessed;
		PreprocessStream(preprocessed);
		m_AnalysisBuffer = std::make_unique<RingBuffer<float>>(preprocessed.size());
		m_AnalysisBuffer->Write(preprocessed.data(), preprocessed.size());

		m_Pipeline = std::make_unique<AnalysisPipeline>();
		m_Pipeline->AddStage(std::make_unique<FFTProcessor>());
	}

	void AnalysisService::Reset()
	{
		StopAnalysis();
		m_Decoder.reset();
		m_Params = AnalysisParams{};
		m_AudioSource.reset();
		m_Pipeline.reset();
	}

	void AnalysisService::StartAnalysis()
	{
		m_Running = true;
		m_AnalysisThread = std::thread([this]()
		{
			std::unique_ptr<AnalysisResult> result = nullptr;
			std::vector<std::vector<float>> rollingAvg;
			while (m_Running)
			{
				result = ProcessCurrentFrame();
				if (m_RollingAvgCount > 1)
				{
					auto& data = result->Magnitudes;
					rollingAvg.push_back(data);
					if (rollingAvg.size() > m_RollingAvgCount)
						rollingAvg.erase(rollingAvg.begin());
					for (size_t i = 0; i < data.size(); i++)
					{
						float sum = 0.0f;
						for (const auto& vec : rollingAvg)
							sum += vec[i];
						data[i] = sum / static_cast<float>(rollingAvg.size());
					}
					result->Magnitudes = data;
				}
				nlohmann::json json = AnalysisPipeline::GetResultJson(*result);
				MessageQueue::Instance().Push(json.dump());
				std::this_thread::sleep_for(std::chrono::milliseconds(m_IntervalMs));
			}
		});
	}

	void AnalysisService::StopAnalysis()
	{
		m_Running = false;
		if (m_AnalysisThread.joinable())
			m_AnalysisThread.join();
	}

	void AnalysisService::RequestCurrentFrameAnalysis()
	{
		std::unique_ptr<AnalysisResult> result = ProcessCurrentFrame();
		nlohmann::json json = AnalysisPipeline::GetResultJson(*result);
		MessageQueue::Instance().Push(json.dump());
	}

	std::unique_ptr<AnalysisResult> AnalysisService::ProcessCurrentFrame()
	{
		double deltaTime = std::chrono::high_resolution_clock::now().time_since_epoch().count() / 1e9 - m_Decoder->GetLastPlaybackFrameTimestamp();
		int currentFrameStart = std::clamp(static_cast<int>((m_Decoder->GetPlaybackTime() + deltaTime) * m_Params.SampleRate - m_Params.FrameLength / static_cast<float>(2)), 0, (int)m_AnalysisBuffer->GetCapacity());
		kfr::univector<float> samples(m_Params.FrameLength);
		size_t samplesRead = m_AnalysisBuffer->Read(samples.data(), m_Params.FrameLength, currentFrameStart);

		AudioFrame frame;
		frame.SampleRate = static_cast<uint32_t>(m_Params.SampleRate);
		frame.Samples = samples;
		return m_Pipeline->ProcessFrame(frame);
	}

	void AnalysisService::PreprocessStream(kfr::univector<float>& outStream)
	{
		auto& sourcePcm = m_AudioSource->PCMData;
		size_t sourceSamples = m_AudioSource->SamplesPerChannel;
		int channels = m_AudioSource->Channels;

		// Mix down to mono. If multichannel, average channels.
		kfr::univector<float> mono(sourceSamples);
		if (channels <= 1)
		{
			if (sourceSamples > 0)
				std::copy(sourcePcm[0].begin(), sourcePcm[0].begin() + sourceSamples, mono.begin());
		}
		else
		{
			for (size_t i = 0; i < sourceSamples; ++i)
			{
				float sum = 0.0f;
				for (int c = 0; c < channels; ++c)
					sum += sourcePcm[c][i];
				mono[i] = sum / static_cast<float>(channels);
			}
		}
		if (m_Params.SampleRate == m_AudioSource->SampleRate)
		{
			outStream = std::move(mono);
			return;
		}
		// Filter to prevent aliasing before resampling
		auto filterParams = kfr::to_sos<float>(kfr::iir_lowpass(kfr::butterworth<float>(12), m_Params.SampleRate / 2.0f, m_AudioSource->SampleRate));
		kfr::univector<float> filtered = kfr::iir(mono, filterParams);

		// Resample to target sample rate
		kfr::samplerate_converter<float> resampler = kfr::resampler<float>(kfr::resample_quality::high, m_Params.SampleRate, m_AudioSource->SampleRate);
		size_t outputSamples = resampler.output_size_for_input(sourceSamples);
		outStream.resize(outputSamples);
		resampler.process(outStream, filtered);
	}
}
