#pragma once
#include "AudioData.h"

#include <atomic>
#include <memory>
#include "miniaudio.h"

namespace Adagio
{
	class PcmFeeder;
	class TimeProcessor;

	template<typename T>
	class RingBuffer;

	class PlaybackService
	{
	public:
		PlaybackService();
		~PlaybackService();

		void OnAudioCallback(float* outBuffer, ma_uint32 framesToRead);

		int Init(std::shared_ptr<PcmFeeder> feeder);
		void Reset();
		void PlayAudio();
		void PauseAudio();
		void StopAudio();
		void SetVolume(float volume);
		void SeekToSample(uint64_t sample);
		void SetSpeed(float speed);

		float GetVolume() const { return m_Volume.load(std::memory_order_acquire); }
		float GetSpeed() const;
		double GetPositionSeconds() const;
		uint64_t GetPositionSamples() const;
		int GetUnderrunCount() const { return m_UnderrunCount.load(std::memory_order_acquire); }
		float GetMaxCallbackUs() const { return m_MaxCallbackUs.load(std::memory_order_acquire); }

		bool ConsumeEndOfPlay() { return m_EndReported.exchange(false, std::memory_order_acq_rel); }

	private:
		void UninitDevice();

		std::shared_ptr<PcmFeeder> m_Feeder;
		// Owned by the feeder; cached to keep name lookups out of the audio callback.
		RingBuffer<float>* m_PlaybackBuffer = nullptr;
		ma_device m_PlaybackDevice{};

		bool m_DeviceInitialised = false;

		std::shared_ptr<AudioData> m_AudioSource;
		std::atomic<float> m_Volume{ 0.2f };
		std::atomic<uint64_t> m_CurrentPlaybackFrame{ 0 };
		std::atomic<int> m_PlaybackUpdateCounter{ 0 };
		std::atomic<bool> m_EndReported{ false };
		// Audio thread only: the seek generation this callback has already acted on.
		uint32_t m_SeekGenerationSeen = 0;
		std::unique_ptr<TimeProcessor> m_TimeProcessor;

		std::atomic<int> m_UnderrunCount{ 0 };
		std::atomic<float> m_MaxCallbackUs{ 0.0f };
	};
}
