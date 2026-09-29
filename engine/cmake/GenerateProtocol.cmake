# Turns protocol/protocol.json into a header of constants, so the C++ side spells
# every command, event and transport state exactly the way the renderer does.
#
# The JSON is the one source of truth: the UI imports the same file. Adding a
# command means editing the JSON, nothing else - the enum, the name table and the
# argument kinds all come from this generator.

function(adagio_capitalise input output)
	string(SUBSTRING "${input}" 0 1 first)
	string(SUBSTRING "${input}" 1 -1 rest)
	string(TOUPPER "${first}" first)
	set(${output} "${first}${rest}" PARENT_SCOPE)
endfunction()

# CMake reads JSON numbers as doubles and prints them back with every digit, so 0.2
# arrives as 0.20000000000000001. Six decimals is far more than a float literal can
# hold on to, and keeps the generated header readable.
function(adagio_format_float input output)
	string(REGEX MATCH "^-?[0-9]+(\\.[0-9]?[0-9]?[0-9]?[0-9]?[0-9]?[0-9]?)?" value "${input}")
	if(NOT value MATCHES "\\.")
		set(value "${value}.0")
	endif()
	string(REGEX REPLACE "0+$" "" value "${value}")
	if(value MATCHES "\\.$")
		set(value "${value}0")
	endif()
	set(${output} "${value}" PARENT_SCOPE)
endfunction()

function(adagio_generate_protocol_header json_path header_path)
	file(READ "${json_path}" protocol)

	string(JSON version GET "${protocol}" version)
	string(JSON transport GET "${protocol}" transport)
	string(JSON host GET "${transport}" host)
	string(JSON port GET "${transport}" port)
	string(JSON token_param GET "${transport}" tokenParam)
	string(JSON envelope GET "${protocol}" envelope)
	string(JSON reply_type GET "${envelope}" replyType)
	string(JSON limits GET "${protocol}" limits)
	string(JSON volume_min GET "${limits}" volumeMin)
	string(JSON volume_max GET "${limits}" volumeMax)
	string(JSON speed_min GET "${limits}" speedMin)
	string(JSON speed_max GET "${limits}" speedMax)
	adagio_format_float("${volume_min}" volume_min)
	adagio_format_float("${volume_max}" volume_max)
	adagio_format_float("${speed_min}" speed_min)
	adagio_format_float("${speed_max}" speed_max)

	set(commands "")
	string(JSON command_count LENGTH "${protocol}" commands)
	math(EXPR last "${command_count} - 1")
	foreach(index RANGE ${last})
		string(JSON entry GET "${protocol}" commands ${index})
		string(JSON name GET "${entry}" name)
		string(JSON cpp GET "${entry}" cpp)
		string(JSON arg GET "${entry}" arg)
		adagio_capitalise("${arg}" arg)
		string(APPEND commands "\tX(${cpp}, \"${name}\", ${arg}) \\\n")
	endforeach()

	set(events "")
	string(JSON event_count LENGTH "${protocol}" events)
	math(EXPR last "${event_count} - 1")
	foreach(index RANGE ${last})
		string(JSON entry GET "${protocol}" events ${index})
		string(JSON name GET "${entry}" name)
		string(JSON cpp GET "${entry}" cpp)
		string(APPEND events "\t\t\tconstexpr const char* ${cpp} = \"${name}\";\n")
	endforeach()

	set(states "")
	string(JSON state_count LENGTH "${protocol}" states)
	math(EXPR last "${state_count} - 1")
	foreach(index RANGE ${last})
		string(JSON entry GET "${protocol}" states ${index})
		string(JSON name GET "${entry}" name)
		string(JSON cpp GET "${entry}" cpp)
		string(APPEND states "\tX(${cpp}, \"${name}\") \\\n")
	endforeach()

	string(JSON binary GET "${protocol}" binaryFrames)
	string(JSON binary_header_size GET "${binary}" headerSize)

	set(binary_kinds "")
	string(JSON kind_count LENGTH "${binary}" kinds)
	math(EXPR last "${kind_count} - 1")
	foreach(index RANGE ${last})
		string(JSON entry GET "${binary}" kinds ${index})
		string(JSON cpp GET "${entry}" cpp)
		string(JSON value GET "${entry}" value)
		string(APPEND binary_kinds "\t\t\t\t${cpp} = ${value},\n")
	endforeach()

	set(binary_element_types "")
	string(JSON element_type_count LENGTH "${binary}" elementTypes)
	math(EXPR last "${element_type_count} - 1")
	foreach(index RANGE ${last})
		string(JSON entry GET "${binary}" elementTypes ${index})
		string(JSON cpp GET "${entry}" cpp)
		string(JSON value GET "${entry}" value)
		string(APPEND binary_element_types "\t\t\t\t${cpp} = ${value},\n")
	endforeach()

	set(binary_offsets "")
	string(JSON field_count LENGTH "${binary}" header)
	math(EXPR last "${field_count} - 1")
	foreach(index RANGE ${last})
		string(JSON entry GET "${binary}" header ${index})
		string(JSON cpp GET "${entry}" cpp)
		string(JSON offset GET "${entry}" byteOffset)
		string(APPEND binary_offsets "\t\t\t\tconstexpr size_t ${cpp} = ${offset};\n")
	endforeach()

	set(allowed_origins "")
	string(JSON origin_count LENGTH "${transport}" allowedOrigins)
	math(EXPR last "${origin_count} - 1")
	foreach(index RANGE ${last})
		string(JSON origin GET "${transport}" allowedOrigins ${index})
		string(APPEND allowed_origins "\t\t\t\"${origin}\",\n")
	endforeach()

	set(generated "// Generated from protocol/protocol.json by engine/cmake/GenerateProtocol.cmake.
// Edit the JSON, not this file. Re-runs whenever the JSON changes.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

// X(CppName, wireName, ArgumentKind)
#define ADAGIO_PROTOCOL_COMMANDS(X) \\
${commands}
// X(CppName, wireName)
#define ADAGIO_PROTOCOL_STATES(X) \\
${states}
namespace Adagio
{
	namespace Protocol
	{
		constexpr int Version = ${version};
		constexpr const char* Host = \"${host}\";
		constexpr int Port = ${port};
		constexpr const char* TokenParam = \"${token_param}\";
		constexpr const char* ReplyType = \"${reply_type}\";

		constexpr float VolumeMin = ${volume_min}f;
		constexpr float VolumeMax = ${volume_max}f;
		constexpr float SpeedMin = ${speed_min}f;
		constexpr float SpeedMax = ${speed_max}f;

		// A browser page always sends an Origin; anything not on this list is not the
		// renderer and is refused before it can queue a command.
		constexpr std::array<std::string_view, ${origin_count}> AllowedOrigins = {
${allowed_origins}		};

		namespace Event
		{
${events}		}

		// Binary frames: a fixed header, then Count elements at HeaderSize.
		namespace Binary
		{
			constexpr size_t HeaderSize = ${binary_header_size};

			enum class FrameKind : uint8_t
			{
${binary_kinds}			};

			enum class ElementType : uint16_t
			{
${binary_element_types}			};

			namespace Offset
			{
${binary_offsets}			}
		}
	}
}
")

	# Only rewrite when something changed, so touching the JSON does not rebuild the
	# world unless the generated text actually differs.
	set(previous "")
	if(EXISTS "${header_path}")
		file(READ "${header_path}" previous)
	endif()
	if(NOT previous STREQUAL generated)
		file(WRITE "${header_path}" "${generated}")
	endif()

	# Re-run CMake when the protocol changes, which regenerates the header above.
	set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${json_path}")
endfunction()
