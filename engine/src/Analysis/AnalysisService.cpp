#include "AnalysisService.h"
#include "AnalysisPipeline.h"
#include "ChordPredictor.h"
#include "FFTProcessor.h"
#include "HPSDownsamplerProcessor.h"
#include "KeyDetector.h"
#include "NoteDetector.h"
#include "PeakExtractor.h"
#include "SpectrumFilterProcessor.h"
#include "../Core/AudioDecoder.h"
#include "../Core/MessageQueue.h"
#include "../IO/AudioData.h"
#include "../IO/PlaybackService.h"

#include <kfr/dsp.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>

namespace Adagio
{
	AnalysisService::AnalysisService()
		: m_Running(false), m_IntervalMs(5), m_RollingAvgCount(4), m_AnalysisBuffer(nullptr)
	{
	}

	AnalysisService::~AnalysisService()
	{
	}

	void AnalysisService::Init(std::shared_ptr<AudioDecoder> decoder, AnalysisParams params, const PlaybackService* playback)
	{
		StopAnalysis();
		m_Decoder = decoder;
		m_Playback = playback;
		m_Params = params;
		m_AudioSource = m_Decoder->GetAudioSource();

		kfr::univector<float> preprocessed;
		PreprocessStream(preprocessed);
		m_AnalysisBuffer = std::make_unique<RingBuffer<float>>(preprocessed.size());
		m_AnalysisBuffer->Write(preprocessed.data(), preprocessed.size());

		m_Pipeline = std::make_unique<AnalysisPipeline>();
		m_Pipeline->AddStage(std::make_unique<FFTProcessor>());
		m_Pipeline->AddStage(std::make_unique<HPSDownsamplerProcessor>());
		m_Pipeline->AddStage(std::make_unique<SpectrumFilterProcessor>());
		m_Pipeline->AddStage(std::make_unique<PeakExtractor>());
		m_Pipeline->AddStage(std::make_unique<NoteDetector>());
		m_Pipeline->AddStage(std::make_unique<KeyDetector>());
		m_Pipeline->AddStage(std::make_unique<ChordPredictor>());
	}

	void AnalysisService::Reset()
	{
		StopAnalysis();
		m_Decoder.reset();
		m_Playback = nullptr;
		m_Params = AnalysisParams{};
		m_AudioSource.reset();
		m_Pipeline.reset();
		m_AnalysisBuffer.reset();
	}

	void AnalysisService::StartAnalysis()
	{
		// Prevent a second StartAnalysis from starting a second thread.
		if (m_Running.exchange(true, std::memory_order_acq_rel))
			return;
		if (!m_Pipeline || !m_AnalysisBuffer)
		{
			m_Running.store(false, std::memory_order_release);
			return;
		}

		m_AnalysisThread = std::thread([this]()
		{
			std::unique_ptr<AnalysisResult> result = nullptr;
			std::vector<kfr::univector<float>> rollingAvg;
			while (m_Running)
			{
				if (SyncSeekGeneration())
					rollingAvg.clear();
				result = ProcessCurrentFrame();
				auto data = result->Context->Magnitudes;
				if (m_RollingAvgCount > 1)
				{
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
					result->Context->Magnitudes = data;
				}
				nlohmann::json json = AnalysisPipeline::GetResultJson(*result);
				MessageQueue::GetInstance().Push(json.dump());
				int uSSleepTime = (m_IntervalMs - result->ExecutionTimeMs) * 1000;
				std::this_thread::sleep_for(std::chrono::microseconds(uSSleepTime));
			}
		});
	}

	void AnalysisService::StopAnalysis()
	{
		m_Running.store(false, std::memory_order_release);
		if (m_AnalysisThread.joinable())
			m_AnalysisThread.join();
	}

	void AnalysisService::RequestCurrentFrameAnalysis()
	{
		// Ensure that the analysis thread is not running before making an adhoc request
		if (m_Running.load(std::memory_order_acquire) || !m_Pipeline || !m_AnalysisBuffer)
			return;

		SyncSeekGeneration();
		PublishCurrentFrame();
	}

	void AnalysisService::PublishCurrentFrame()
	{
		std::unique_ptr<AnalysisResult> result = ProcessCurrentFrame();
		nlohmann::json json = AnalysisPipeline::GetResultJson(*result);
		MessageQueue::GetInstance().Push(json.dump());
	}

	bool AnalysisService::SyncSeekGeneration()
	{
		const uint32_t generation = m_Decoder->GetSeekGeneration();
		if (generation == m_SeekGenerationSeen)
			return false;

		m_SeekGenerationSeen = generation;
		m_Pipeline->ResetPersistentData();
		return true;
	}

	double AnalysisService::ExtrapolatePlayhead(double playbackTime, double lastFrameTimestamp, double now, double speed, bool playing)
	{
		if (!playing || lastFrameTimestamp <= 0.0)
			return playbackTime;

		constexpr double maxExtrapolationSeconds = 0.25;
		const double wallDelta = std::clamp(now - lastFrameTimestamp, 0.0, maxExtrapolationSeconds);
		return playbackTime + wallDelta * speed;
	}

	std::unique_ptr<AnalysisResult> AnalysisService::ProcessCurrentFrame()
	{
		const double now = std::chrono::high_resolution_clock::now().time_since_epoch().count() / 1e9;
		const double speed = m_Playback ? m_Playback->GetSpeed() : 1.0;

		m_AnalysisTimestamp = ExtrapolatePlayhead(m_Decoder->GetPlaybackTime(), m_Decoder->GetLastPlaybackFrameTimestamp(), now, speed, m_Running.load(std::memory_order_acquire));
		int currentFrameStart = std::clamp(static_cast<int>(m_AnalysisTimestamp * m_Params.SampleRate - m_Params.FrameLength / static_cast<float>(2)), 0, (int)m_AnalysisBuffer->GetCapacity());
		kfr::univector<float> samples(m_Params.FrameLength);
		size_t samplesRead = m_AnalysisBuffer->Read(samples.data(), m_Params.FrameLength, currentFrameStart);

		AudioFrame frame;
		frame.SampleRate = static_cast<uint32_t>(m_Params.SampleRate);
		frame.FrameLength = static_cast<uint32_t>(m_Params.FrameLength);
		frame.Timestamp = m_AnalysisTimestamp;
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
