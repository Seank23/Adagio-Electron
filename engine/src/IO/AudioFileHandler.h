#pragma once
#include <string>

namespace Adagio
{
	class AudioData;

	class AudioFileHandler
	{
	public:
		// Each of these throws std::runtime_error if the file cannot be decoded or
		// decodes to nothing, so the caller never has to inspect an empty AudioData.
		void ReadWAV(const std::string& fileName, AudioData& o_Audio) const;
		void ReadMP3(const std::string& fileName, AudioData& o_Audio) const;
		void ReadFLAC(const std::string& fileName, AudioData& o_Audio) const;
	};
}
