#include "FileIOService.h"
#include "AudioData.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <stdexcept>

namespace Adagio
{
	FileIOService::FileIOService()
	{
		m_AudioFileHandler = std::make_unique<AudioFileHandler>();
	}

	FileFormat FileIOService::FormatFromPath(const std::string& filepath)
	{
		if (filepath.empty())
			return FileFormat::Unknown;

		std::string extension = std::filesystem::path(filepath).extension().string();
		std::transform(extension.begin(), extension.end(), extension.begin(),
			[](unsigned char c) { return static_cast<char>(std::tolower(c)); });

		if (extension == ".wav")
			return FileFormat::WAV;
		if (extension == ".mp3")
			return FileFormat::MP3;
		if (extension == ".flac")
			return FileFormat::FLAC;
		return FileFormat::Unknown;
	}

	void FileIOService::LoadAudio(const std::string& filepath, FileFormat format, AudioData& o_Audio) const
	{
		switch (format)
		{
		case FileFormat::WAV:
			m_AudioFileHandler->ReadWAV(filepath, o_Audio);
			return;
		case FileFormat::MP3:
			m_AudioFileHandler->ReadMP3(filepath, o_Audio);
			return;
		case FileFormat::FLAC:
			m_AudioFileHandler->ReadFLAC(filepath, o_Audio);
			return;
		case FileFormat::Unknown:
			break;
		}
		throw std::runtime_error("Unsupported audio format: '" + filepath + "'. Supported: .wav, .mp3, .flac.");
	}
}
