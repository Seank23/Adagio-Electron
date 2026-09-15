#include "TimeProcessor.h"
#include "../Buffers/RingBuffer.h"
#include "rubberband/RubberBandStretcher.h"

#include <algorithm>
#include <cstring>

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
		m_OutputCapacityFrames = maxFrames * 2;

		m_InputBufferInterleaved.assign(maxFrames * static_cast<size_t>(channels), 0.0f);

		m_InputChannels.assign(channels, std::vector<float>(maxFrames, 0.0f));
		m_OutputChannels.assign(channels, std::vector<float>(m_OutputCapacityFrames, 0.0f));

		m_InputPtrs.resize(channels);
		m_OutputPtrs.resize(channels);
		for (int ch = 0; ch < channels; ++ch)
		{
			m_InputPtrs[ch] = m_InputChannels[ch].data();
			m_OutputPtrs[ch] = m_OutputChannels[ch].data();
		}

		m_PendingSpeed.store(1.0, std::memory_order_release);
		m_AppliedSpeed.store(1.0, std::memory_order_release);
		m_ResetRequested.store(false, std::memory_order_release);
		m_InStretchMode = false;
		m_FinalBlockSent = false;
		m_LatencyToDiscard = 0;
	}

	void TimeProcessor::Reset()
	{
		m_PendingSpeed.store(1.0, std::memory_order_release);
		m_AppliedSpeed.store(1.0, std::memory_order_release);
		m_InputBuffer = nullptr;
		m_InStretchMode = false;
		m_FinalBlockSent = false;
		m_LatencyToDiscard = 0;
		if (m_Stretcher)
			m_Stretcher->reset();
	}

	void TimeProcessor::ResetStretcher()
	{
		m_ResetRequested.store(true, std::memory_order_release);
	}

	void TimeProcessor::SetSpeed(float speed)
	{
		m_PendingSpeed.store(static_cast<double>(speed), std::memory_order_release);
	}

	ProcessAudioResult TimeProcessor::ProcessAudio(float* output, size_t samplesRequested, bool sourceExhausted)
	{
		ProcessAudioResult result;
		if (!m_InputBuffer || m_Channels <= 0)
		{
			std::memset(output, 0, samplesRequested * sizeof(float));
			return result;
		}

		const double speed = m_PendingSpeed.load(std::memory_order_acquire);
		const bool speedChanged = speed != m_AppliedSpeed.load(std::memory_order_relaxed);
		const bool resetRequested = m_ResetRequested.exchange(false, std::memory_order_acq_rel);
		if (speedChanged)
			m_AppliedSpeed.store(speed, std::memory_order_release);

		if (speed == 1.0)
		{
			m_InStretchMode = false;
			const size_t read = m_InputBuffer->Read(output, samplesRequested);
			if (read < samplesRequested)
				std::memset(output + read, 0, (samplesRequested - read) * sizeof(float));
			result.FramesConsumed = read / static_cast<size_t>(m_Channels);
			result.Drained = sourceExhausted && read < samplesRequested;
			return result;
		}

		if (!m_InStretchMode || speedChanged || resetRequested)
		{
			m_Stretcher->reset();
			m_Stretcher->setTimeRatio(1.0 / speed);
			m_InStretchMode = true;
			m_FinalBlockSent = false;
			m_LatencyToDiscard = static_cast<int64_t>(m_Stretcher->getLatency());
		}

		const size_t framesRequested = samplesRequested / static_cast<size_t>(m_Channels);
		int64_t framesConsumed = 0;
		size_t framesProduced = 0;

		const auto silenceRemainder = [&]()
		{
			const size_t remaining = (framesRequested - framesProduced) * static_cast<size_t>(m_Channels);
			std::memset(output + framesProduced * m_Channels, 0, remaining * sizeof(float));
		};

		while (framesProduced < framesRequested)
		{
			size_t framesRead = 0;

			if (!m_FinalBlockSent)
			{
				const size_t framesNeeded = std::min(m_Stretcher->getSamplesRequired(), m_MaxFramesPerBlock);
				if (framesNeeded > 0)
				{
					const size_t samplesNeeded = framesNeeded * static_cast<size_t>(m_Channels);
					const size_t samplesRead = m_InputBuffer->Read(m_InputBufferInterleaved.data(), samplesNeeded);
					framesRead = samplesRead / static_cast<size_t>(m_Channels);

					for (size_t i = 0; i < framesRead; ++i)
					{
						for (int ch = 0; ch < m_Channels; ++ch)
							m_InputChannels[ch][i] = m_InputBufferInterleaved[i * m_Channels + ch];
					}

					if (framesRead > 0)
					{
						m_Stretcher->process(m_InputPtrs.data(), framesRead, false);
						framesConsumed += static_cast<int64_t>(framesRead);
					}
				}

				// RubberBand holds back its start delay until told the input has ended.
				if (sourceExhausted && m_InputBuffer->GetAvailableCount() == 0)
				{
					m_Stretcher->process(m_InputPtrs.data(), 0, true);
					m_FinalBlockSent = true;
				}
			}

			// -1 once the final block has been fully retrieved.
			const int framesAvailable = m_Stretcher->available();
			if (framesAvailable < 0)
			{
				silenceRemainder();
				result.Drained = true;
				break;
			}
			if (framesAvailable == 0)
			{
				if (framesRead == 0)
				{
					silenceRemainder();
					break;
				}
				continue;
			}

			size_t framesToOutput = std::min(static_cast<size_t>(framesAvailable), framesRequested - framesProduced);
			framesToOutput = std::min(framesToOutput, m_OutputCapacityFrames);

			const size_t framesOut = m_Stretcher->retrieve(m_OutputPtrs.data(), framesToOutput);
			for (size_t i = 0; i < framesOut; ++i)
			{
				for (int ch = 0; ch < m_Channels; ++ch)
					output[(framesProduced + i) * m_Channels + ch] = m_OutputChannels[ch][i];
			}
			framesProduced += framesOut;

			if (framesOut == 0 && framesRead == 0)
			{
				silenceRemainder();
				break;
			}
		}

		int64_t consumed = framesConsumed;
		if (m_LatencyToDiscard > 0)
		{
			const int64_t discard = std::min(m_LatencyToDiscard, consumed);
			consumed -= discard;
			m_LatencyToDiscard -= discard;
		}
		result.FramesConsumed = static_cast<size_t>(std::max<int64_t>(consumed, 0));
		return result;
	}
}
