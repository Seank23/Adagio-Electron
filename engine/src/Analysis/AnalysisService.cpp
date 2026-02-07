#include "AnalysisService.h"
#include "AnalysisPipeline.h"
#include "../Core/AudioDecoder.h"
#include "FFTProcessor.h"
#include "../Core/MessageQueue.h"
#include "../IO/AudioData.h"

#include "kfr/base.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <chrono>

namespace Adagio
{
	AnalysisService::AnalysisService()
		: m_Running(false), m_IntervalMs(50), m_AnalysisBuffer(nullptr)
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

		m_AnalysisBuffer = m_Decoder->GetBuffer("Analysis");
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
			while (m_Running)
			{
				std::unique_ptr<AnalysisResult> result = ProcessCurrentFrame();
				std::cout << "Analyzed frame: Magnitudes size = " << result->Magnitudes.size() << ", SampleRate = " << result->SampleRate << std::endl;
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
		AudioFrame frame;
		frame.SampleRate = static_cast<uint32_t>(m_Params.SampleRate);
		int channels = m_AudioSource->Channels;

		const size_t samplesRequested = static_cast<size_t>(m_Params.FrameLength);
		kfr::univector<float> outBuffer;
		outBuffer.resize(samplesRequested * channels);
		uint32_t samplesRead = m_AnalysisBuffer->Read(outBuffer.data(), static_cast<uint32_t>(samplesRequested * channels));
		outBuffer.resize(samplesRead);

		// Mix down to mono. If multichannel, deinterleave then average channels.
		kfr::univector<float> mono;
		mono.resize(samplesRequested);

		if (channels <= 1)
		{
			// copy samples and zero-pad if needed
			size_t toCopy = std::min<size_t>(samplesRead, samplesRequested);
			if (toCopy > 0)
				std::copy(outBuffer.begin(), outBuffer.begin() + toCopy, mono.begin());
			if (toCopy < samplesRequested)
				std::fill(mono.begin() + toCopy, mono.end(), 0.0f);
		}
		else
		{
			size_t framesRead = samplesRead / static_cast<size_t>(channels);
			kfr::univector2d<float> deinterleaved;
			deinterleaved.resize(channels);
			for (int c = 0; c < channels; ++c)
				deinterleaved[c].resize(framesRead);
			kfr::deinterleave<float>(deinterleaved, outBuffer);
			for (size_t i = 0; i < framesRead; ++i)
			{
				float sum = 0.0f;
				for (int c = 0; c < channels; ++c)
					sum += deinterleaved[c][i];
				mono[i] = sum / static_cast<float>(channels);
			}
			if (framesRead < samplesRequested)
				std::fill(mono.begin() + framesRead, mono.end(), 0.0f);
		}
		frame.Samples = std::move(mono);
		return m_Pipeline->ProcessFrame(frame);
	}
}
