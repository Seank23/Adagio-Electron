#include "AudioFileHandler.h"
#include "AudioData.h"

#include <kfr/base.hpp>
#include <kfr/io.hpp>

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

    const void AudioFileHandler::ReadWAV(std::string fileName, AudioData& o_Audio) const
    {
#if defined(_WIN32)
        const std::wstring wide = Utf8ToWide(fileName);
        kfr::audio_reader_wav<float>* reader = new kfr::audio_reader_wav<float>(kfr::open_file_for_reading(wide));
#else
        kfr::audio_reader_wav<float>* reader = new kfr::audio_reader_wav<float>(kfr::open_file_for_reading(fileName));
#endif
        o_Audio.Channels = reader->format().channels;
        o_Audio.SampleRate = reader->format().samplerate;
        o_Audio.PCMData = reader->read_channels();
        o_Audio.SamplesPerChannel = o_Audio.PCMData[0].size();
        o_Audio.Duration = o_Audio.PCMData[0].size() / o_Audio.SampleRate;
        delete reader;
    }

    const void AudioFileHandler::ReadMP3(std::string fileName, AudioData& o_Audio) const
    {
#if defined(_WIN32)
        const std::wstring wide = Utf8ToWide(fileName);
        kfr::audio_reader_mp3<float>* reader = new kfr::audio_reader_mp3<float>(kfr::open_file_for_reading(wide));
#else
        kfr::audio_reader_mp3<float>* reader = new kfr::audio_reader_mp3<float>(kfr::open_file_for_reading(fileName));
#endif
        o_Audio.Channels = reader->format().channels;
        o_Audio.SampleRate = reader->format().samplerate;
        o_Audio.PCMData = reader->read_channels();
        o_Audio.SamplesPerChannel = o_Audio.PCMData[0].size();
        o_Audio.Duration = o_Audio.PCMData[0].size() / o_Audio.SampleRate;
        delete reader;
    }

    const void AudioFileHandler::ReadFLAC(std::string fileName, AudioData& o_Audio) const
    {
#if defined(_WIN32)
        const std::wstring wide = Utf8ToWide(fileName);
        kfr::audio_reader_flac<float>* reader = new kfr::audio_reader_flac<float>(kfr::open_file_for_reading(wide));
#else
        kfr::audio_reader_flac<float>* reader = new kfr::audio_reader_flac<float>(kfr::open_file_for_reading(fileName));
#endif
        o_Audio.Channels = reader->format().channels;
        o_Audio.SampleRate = reader->format().samplerate;
        o_Audio.PCMData = reader->read_channels();
        o_Audio.SamplesPerChannel = o_Audio.PCMData[0].size();
        o_Audio.Duration = o_Audio.PCMData[0].size() / o_Audio.SampleRate;
        delete reader;
    }
}
