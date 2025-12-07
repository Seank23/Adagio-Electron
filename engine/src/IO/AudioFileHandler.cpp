#include "AudioFileHandler.h"
#include "AudioData.h"

#include <kfr/base.hpp>
#include <kfr/io.hpp>

namespace Adagio
{
    const void AudioFileHandler::ReadWAV(std::string fileName, AudioData& o_Audio) const
    {
        kfr::audio_reader_wav<float>* reader = new kfr::audio_reader_wav<float>(kfr::open_file_for_reading(fileName));
        o_Audio.Channels = reader->format().channels;
        o_Audio.SampleRate = reader->format().samplerate;
        o_Audio.PCMData = reader->read_channels();
        o_Audio.SamplesPerChannel = o_Audio.PCMData[0].size();
        o_Audio.Duration = o_Audio.PCMData[0].size() / o_Audio.SampleRate;
        delete reader;
    }

    const void AudioFileHandler::ReadMP3(std::string fileName, AudioData& o_Audio) const
    {
        kfr::audio_reader_mp3<float>* reader = new kfr::audio_reader_mp3<float>(kfr::open_file_for_reading(fileName));
        o_Audio.Channels = reader->format().channels;
        o_Audio.SampleRate = reader->format().samplerate;
        o_Audio.PCMData = reader->read_channels();
        o_Audio.SamplesPerChannel = o_Audio.PCMData[0].size();
        o_Audio.Duration = o_Audio.PCMData[0].size() / o_Audio.SampleRate;
        delete reader;
    }

    const void AudioFileHandler::ReadFLAC(std::string fileName, AudioData& o_Audio) const
    {
        kfr::audio_reader_flac<float>* reader = new kfr::audio_reader_flac<float>(kfr::open_file_for_reading(fileName));
        o_Audio.Channels = reader->format().channels;
        o_Audio.SampleRate = reader->format().samplerate;
        o_Audio.PCMData = reader->read_channels();
        o_Audio.SamplesPerChannel = o_Audio.PCMData[0].size();
        o_Audio.Duration = o_Audio.PCMData[0].size() / o_Audio.SampleRate;
        delete reader;
    }
}
