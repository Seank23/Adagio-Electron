#pragma once

#include "../Buffers/RingBuffer.h"
#include <unordered_map>
#include <memory>
#include <string>
#include <thread>
#include "kfr/base/univector.hpp"

namespace Adagio
{
	class AudioData;

	enum class FeederState
	{
		STOPPED,
		RUNNING,
		TERMINATED
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

		void ResetAudio();
		void Clear();

		RingBuffer<float>* GetBuffer(const std::string& bufferName);
		std::shared_ptr<AudioData> GetAudioSource() { return m_AudioSource; }

	private:
		std::thread m_FeederThread;
		std::shared_ptr<AudioData> m_AudioSource;
		std::unordered_map<std::string, std::unique_ptr<RingBuffer<float>>> m_Buffers;
		kfr::univector<float> m_FeederData;
		std::atomic<FeederState> m_FeederState{ FeederState::STOPPED };
		std::atomic<uint64_t> m_FeederPosition{ 0 };

		size_t m_FramesPerChunk;
		size_t m_SamplesPerChunk;
	};
}