#pragma once
#include <memory>
#include <string>

#include "AudioFileHandler.h"

namespace Adagio
{
	enum FileFormat
	{
		WAV, MP3, FLAC
	};

	class AudioData;

	class FileIOService
	{
	public:
		FileIOService();
        void LoadAudio(std::string filepath, FileFormat format, AudioData& o_Audio) const;

	private:
		std::unique_ptr<AudioFileHandler> m_AudioFileHandler;
	};
}

