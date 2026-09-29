#pragma once
#include "Protocol.generated.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <vector>

namespace Adagio
{
	struct BinaryFrameHeader
	{
		uint8_t Kind = 0;
		uint8_t Version = 0;
		uint16_t ElementType = 0;
		uint32_t SeekGeneration = 0;
		double Timestamp = 0.0;
		float Resolution = 0.0f;
		float MaxMagnitude = 0.0f;
		uint32_t Count = 0;
		uint32_t Reserved = 0;
	};

	static_assert(std::endian::native == std::endian::little);
	static_assert(sizeof(BinaryFrameHeader) == Protocol::Binary::HeaderSize);
	static_assert(offsetof(BinaryFrameHeader, Kind) == Protocol::Binary::Offset::Kind);
	static_assert(offsetof(BinaryFrameHeader, Version) == Protocol::Binary::Offset::Version);
	static_assert(offsetof(BinaryFrameHeader, ElementType) == Protocol::Binary::Offset::ElementType);
	static_assert(offsetof(BinaryFrameHeader, SeekGeneration) == Protocol::Binary::Offset::SeekGeneration);
	static_assert(offsetof(BinaryFrameHeader, Timestamp) == Protocol::Binary::Offset::Timestamp);
	static_assert(offsetof(BinaryFrameHeader, Resolution) == Protocol::Binary::Offset::Resolution);
	static_assert(offsetof(BinaryFrameHeader, MaxMagnitude) == Protocol::Binary::Offset::MaxMagnitude);
	static_assert(offsetof(BinaryFrameHeader, Count) == Protocol::Binary::Offset::Count);
	static_assert(offsetof(BinaryFrameHeader, Reserved) == Protocol::Binary::Offset::Reserved);

	namespace BinaryFrame
	{
		template <typename T>
		std::string Encode(BinaryFrameHeader header, std::span<const T> data)
		{
			header.Version = (uint8_t)Protocol::Version;
			header.Count = (uint32_t)data.size();

			// IXWebSocket sends a binary payload from a std::string, so the frame is built in one.
			std::string frame(Protocol::Binary::HeaderSize + data.size_bytes(), '\0');
			std::memcpy(frame.data(), &header, sizeof(header));
			if (!data.empty())
				std::memcpy(frame.data() + Protocol::Binary::HeaderSize, data.data(), data.size_bytes());
			return frame;
		}

		// Magnitudes as a fraction of their own maximum, which is exactly what the
		// spectrum canvas plots. A step is 1/65535 of full height, far below a pixel.
		inline std::string EncodeSpectrum(std::span<const float> magnitudes, float binHz, double timestamp, uint32_t seekGeneration)
		{
			float maxMagnitude = 0.0f;
			for (float magnitude : magnitudes)
				maxMagnitude = std::max(maxMagnitude, magnitude);

			std::vector<uint16_t> quantised(magnitudes.size(), 0);
			if (maxMagnitude > 0.0f)
			{
				const float scale = 65535.0f / maxMagnitude;
				for (size_t i = 0; i < magnitudes.size(); ++i)
					quantised[i] = (uint16_t)std::clamp(magnitudes[i] * scale + 0.5f, 0.0f, 65535.0f);
			}

			BinaryFrameHeader header;
			header.Kind = (uint8_t)Protocol::Binary::FrameKind::Spectrum;
			header.ElementType = (uint16_t)Protocol::Binary::ElementType::Uint16;
			header.SeekGeneration = seekGeneration;
			header.Timestamp = timestamp;
			header.Resolution = binHz;
			header.MaxMagnitude = maxMagnitude;
			return Encode<uint16_t>(header, quantised);
		}

		inline std::string EncodeWaveform(std::span<const float> peaks, int framesPerPeak)
		{
			float maxMagnitude = 0.0f;
			for (float peak : peaks)
				maxMagnitude = std::max(maxMagnitude, std::abs(peak));

			BinaryFrameHeader header;
			header.Kind = (uint8_t)Protocol::Binary::FrameKind::Waveform;
			header.ElementType = (uint16_t)Protocol::Binary::ElementType::Float32;
			header.Resolution = (float)framesPerPeak;
			header.MaxMagnitude = maxMagnitude;
			return Encode<float>(header, peaks);
		}
	}
}
