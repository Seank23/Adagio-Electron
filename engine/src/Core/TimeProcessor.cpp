#include "TimeProcessor.h"
#include "../Buffers/RingBuffer.h"
#include "rubberband/RubberBandStretcher.h"	
#include <cassert>
#include <iostream>

namespace Adagio
{
	TimeProcessor::TimeProcessor()
	{
	}

	TimeProcessor::~TimeProcessor()
	{
	}

	void TimeProcessor::Init(int sampleRate, int channels, RingBuffer<float>* buffer, size_t maxFrames)
	{
		m_InputBuffer = buffer;
		m_Channels = channels;
		m_Stretcher = std::make_unique<RubberBand::RubberBandStretcher>(
			sampleRate,
			channels,
			RubberBand::RubberBandStretcher::OptionProcessRealTime |
			RubberBand::RubberBandStretcher::OptionThreadingNever
		);

		m_MaxFramesPerBlock = maxFrames;
		size_t maxSamples = maxFrames * channels;

		m_InputBufferInterleaved.resize(maxSamples);
		m_OutputBufferInterleaved.resize(maxSamples * 2);

		m_InputChannels.resize(channels);
		m_OutputChannels.resize(channels);

		m_InputPtrs.resize(channels, 0);
		m_OutputPtrs.resize(channels, 0);

		for (int ch = 0; ch < channels; ++ch)
		{
			m_InputChannels[ch].resize(maxFrames, 0.0f);
			m_OutputChannels[ch].resize(maxFrames * 2, 0.0f);

			m_InputPtrs[ch] = m_InputChannels[ch].data();
			m_OutputPtrs[ch] = m_OutputChannels[ch].data();
		}
	}

	void TimeProcessor::Reset()
	{
		m_Speed.store(1.0f, std::memory_order_release);
		m_Stretcher->reset();
		m_InputBuffer = nullptr;
	}

	size_t TimeProcessor::ProcessAudio(float* output, size_t samplesRequested)
	{
		if (!m_InputBuffer)
		{
			std::memset(output, 0, samplesRequested * sizeof(float));
			return samplesRequested;
		}

		if (m_Speed == 1.0f)
		{
			m_FirstStretch = true;
			size_t read = m_InputBuffer->Read(output, samplesRequested);
			if (read < samplesRequested)
				std::memset(output + read, 0, (samplesRequested - read) * sizeof(float));
			return read / m_Channels;
		}
		
		size_t latency = m_FirstStretch ? m_Stretcher->getLatency() : 0;
		m_FirstStretch = false;

		size_t framesRequested = samplesRequested / m_Channels;
		size_t framesConsumed = 0;
		size_t framesProduced = 0;

		while (framesProduced < framesRequested)
		{
			size_t framesNeeded = std::min(m_Stretcher->getSamplesRequired(), m_MaxFramesPerBlock);

			if (framesNeeded > 0)
			{
				size_t samplesNeeded = framesNeeded * m_Channels;

				size_t samplesRead = m_InputBuffer->Read(m_InputBufferInterleaved.data(), samplesNeeded);
				if (samplesRead < samplesNeeded)
					std::memset(m_InputBufferInterleaved.data() + samplesRead, 0, (samplesNeeded - samplesRead) * sizeof(float));

				size_t framesRead = samplesRead / m_Channels;
				assert(framesRead <= m_MaxFramesPerBlock);

				std::vector<std::vector<float>> inputChannels(m_Channels, std::vector<float>(framesRead));
				for (size_t i = 0; i < framesRead; ++i)
				{
					for (int ch = 0; ch < m_Channels; ++ch)
						m_InputChannels[ch][i] = m_InputBufferInterleaved[i * m_Channels + ch];
				}

				m_Stretcher->process(m_InputPtrs.data(), framesRead, false);
				framesConsumed += framesRead;
			}

			size_t framesAvailable = m_Stretcher->available();
			if (framesAvailable == 0)
				continue;

			size_t framesToOutput = std::min(framesAvailable, framesRequested - framesProduced);

			size_t framesOut = m_Stretcher->retrieve(m_OutputPtrs.data(), framesToOutput);
			for (size_t i = 0; i < framesOut; ++i)
			{
				for (int ch = 0; ch < m_Channels; ++ch)
					output[(framesProduced + i) * m_Channels + ch] = m_OutputChannels[ch][i];
			}

			framesProduced += framesOut;
		}
		return framesConsumed - latency;
	}

	void TimeProcessor::SetSpeed(float speed)
	{
		m_Speed.store(speed, std::memory_order_release);
		m_Stretcher->setTimeRatio(1.0f / speed);
	}
}