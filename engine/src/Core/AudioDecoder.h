#pragma once

#include "../Buffers/RingBuffer.h"
#include <kfr/base/univector.hpp>

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>

namespace Adagio
{
	class AudioData;

	enum class FeederState
	{
		Stopped,
		Running,
		Terminated
	};

	class AudioDecoder
	{
	public:
		AudioDecoder();
		~AudioDecoder();

		void Init(std::shared_ptr<AudioData> audioData);
		void AddBuffer(const std::string& bufferName, float durationSeconds);
		void LaunchFeeder();
		void SetFeederState(FeederState state) { m_FeederState.store(state, std::memory_order_release); }

		// Seeking is a message to the feeder thread rather than a call into it
		void RequestSeek(uint64_t sample);
		uint64_t GetSeekTargetSample() const { return m_SeekTargetSample.load(std::memory_order_acquire); }
		uint32_t GetSeekGeneration() const { return m_SeekGeneration.load(std::memory_order_acquire); }

		// Tells the end of the track apart from a feeder that is merely running late.
		bool GetIsSourceExhausted(uint32_t generation) const;

		void ResetAudio();
		void Clear();

		RingBuffer<float>* GetBuffer(const std::string& bufferName);
		std::shared_ptr<AudioData> GetAudioSource() { return m_AudioSource; }
		double GetPlaybackTime() const { return m_PlaybackTime.load(std::memory_order_acquire); }
		void SetPlaybackTime(double time) { m_PlaybackTime.store(time, std::memory_order_release); }
		double GetLastPlaybackFrameTimestamp() const { return m_LastPlaybackFrameTimestamp.load(std::memory_order_acquire); }
		void SetLastPlaybackFrameTimestamp(double time) { m_LastPlaybackFrameTimestamp.store(time, std::memory_order_release); }

	private:
		std::thread m_FeederThread;
		std::shared_ptr<AudioData> m_AudioSource;
		std::unordered_map<std::string, std::unique_ptr<RingBuffer<float>>> m_Buffers;
		kfr::univector<float> m_FeederData;
		std::atomic<FeederState> m_FeederState{ FeederState::Stopped };
		std::atomic<uint64_t> m_FeederPosition{ 0 };
		std::atomic<uint64_t> m_TotalSamples{ 0 };
		std::atomic<double> m_PlaybackTime{ 0.0 };
		std::atomic<double> m_LastPlaybackFrameTimestamp{ 0.0 };

		std::atomic<uint64_t> m_SeekTargetSample{ 0 };
		std::atomic<uint32_t> m_SeekGeneration{ 0 };
		std::atomic<uint32_t> m_FeederGeneration{ 0 };

		size_t m_FramesPerChunk;
		size_t m_SamplesPerChunk;
	};
}
