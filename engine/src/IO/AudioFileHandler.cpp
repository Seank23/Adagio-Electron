#include "AudioFileHandler.h"
#include "AudioData.h"

#include <kfr/base.hpp>
#include <kfr/io.hpp>

#include <memory>
#include <stdexcept>

#if defined(_WIN32)
#include <windows.h>
#include <string_view>
#endif

namespace Adagio
{

#if defined(_WIN32)
	static std::wstring Utf8ToWide(std::string_view utf8)
	{
		if (utf8.empty())
			return {};

		const int required = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
		if (required <= 0)
			return {};

		std::wstring out(static_cast<size_t>(required), L'\0');
		::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(), static_cast<int>(utf8.size()), out.data(), required);
		return out;
	}
#endif

	namespace
	{
		template <typename Reader>
		void ReadInto(const std::string& fileName, AudioData& o_Audio)
		{
#if defined(_WIN32)
			auto reader = std::make_unique<Reader>(kfr::open_file_for_reading(Utf8ToWide(fileName)));
#else
			auto reader = std::make_unique<Reader>(kfr::open_file_for_reading(fileName));
#endif
			const int channels = static_cast<int>(reader->format().channels);
			const float sampleRate = static_cast<float>(reader->format().samplerate);
			kfr::univector2d<float> pcm = reader->read_channels();

			if (channels <= 0 || sampleRate <= 0.0f || pcm.empty() || pcm[0].empty())
				throw std::runtime_error("Decoded no audio from '" + fileName + "'.");
			if (static_cast<int>(pcm.size()) < channels)
				throw std::runtime_error("Decoder returned fewer channels than the header declares for '" + fileName + "'.");

			o_Audio.Channels = channels;
			o_Audio.SampleRate = sampleRate;
			o_Audio.PCMData = std::move(pcm);
			o_Audio.SamplesPerChannel = static_cast<int>(o_Audio.PCMData[0].size());
			o_Audio.Duration = o_Audio.PCMData[0].size() / o_Audio.SampleRate;
		}
	}

	void AudioFileHandler::ReadWAV(const std::string& fileName, AudioData& o_Audio) const
	{
		ReadInto<kfr::audio_reader_wav<float>>(fileName, o_Audio);
	}

	void AudioFileHandler::ReadMP3(const std::string& fileName, AudioData& o_Audio) const
	{
		ReadInto<kfr::audio_reader_mp3<float>>(fileName, o_Audio);
	}

	void AudioFileHandler::ReadFLAC(const std::string& fileName, AudioData& o_Audio) const
	{
		ReadInto<kfr::audio_reader_flac<float>>(fileName, o_Audio);
	}
}
