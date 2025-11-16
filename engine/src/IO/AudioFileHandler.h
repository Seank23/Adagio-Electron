#pragma once

#include <kfr/base.hpp>
#include <kfr/io.hpp>
#include "AudioData.h"

namespace Adagio
{
	class AudioFileHandler
	{
	public:
        const void ReadWAV(std::string fileName, AudioData& o_Audio) const;
        const void ReadMP3(std::string fileName, AudioData& o_Audio) const;
        const void ReadFLAC(std::string fileName, AudioData& o_Audio) const;
	};
}
