#pragma once
#include <string>
#include <variant>

enum class CommandType
{
	Play,
	Pause,
	Stop,
	Load,
	Clear,
	Seek,
	SetVolume
};

struct Command
{
	CommandType Type;
	float Value = 0.0f;
	std::string StrValue;
};