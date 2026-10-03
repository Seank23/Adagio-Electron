#pragma once
#include "Command.h"

namespace Adagio
{
	// The single owner of playback state. Empty means no file; Loading covers the
	// decode; Ready is loaded but not started; Playing and Paused swap freely.
	enum class TransportState
	{
#define ADAGIO_STATE_ENUM(cppName, wireName) cppName,
		ADAGIO_PROTOCOL_STATES(ADAGIO_STATE_ENUM)
#undef ADAGIO_STATE_ENUM
	};

	bool HasFile(TransportState state);

	const char* ToString(TransportState state);

	bool IsCommandLegal(TransportState state, CommandType command);
	TransportState NextState(TransportState state, CommandType command);

	const char* RejectionReason(TransportState state, CommandType command);
}
