#include "FileIOService.h"
#include "../Core/AudioUtils.h"

namespace Adagio
{
    FileIOService::FileIOService()
    {
		m_AudioFileHandler = std::make_unique<AudioFileHandler>();
    }

    void FileIOService::LoadAudio(std::string filepath, FileFormat format, AudioData& o_Audio) const
    {
        switch (format)
        {
        case WAV:
            m_AudioFileHandler->ReadWAV(filepath, o_Audio);
            break;
        case MP3:
            m_AudioFileHandler->ReadMP3(filepath, o_Audio);
            break;
        case FLAC:
            m_AudioFileHandler->ReadFLAC(filepath, o_Audio);
            break;
        }
    }
}
