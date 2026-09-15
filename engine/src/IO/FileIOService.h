#pragma once
#include <memory>
#include <string>

#include "AudioFileHandler.h"

namespace Adagio
{
	enum class FileFormat
	{
		Unknown,
		WAV,
		MP3,
		FLAC
	};

	class AudioData;

	class FileIOService
	{
	public:
		FileIOService();

		static FileFormat FormatFromPath(const std::string& filepath);

		// Throws std::runtime_error for an unknown format or an undecodable file.
		void LoadAudio(const std::string& filepath, FileFormat format, AudioData& o_Audio) const;

	private:
		std::unique_ptr<AudioFileHandler> m_AudioFileHandler;
	};
}
