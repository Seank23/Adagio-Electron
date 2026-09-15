// The transport table, exercised over every state x command pair.
//
// IsCommandLegal and NextState are pure, so the whole matrix can be checked without
// an audio device. If a rule changes, the expected table below changes with it and
// the mismatch names the exact cell.
#include "../src/Core/TransportState.h"

#include <doctest/doctest.h>

#include <vector>

namespace
{
	using Adagio::CommandType;
	using Adagio::TransportState;

	const std::vector<TransportState> AllStates = {
		TransportState::Empty,
		TransportState::Loading,
		TransportState::Ready,
		TransportState::Playing,
		TransportState::Paused
	};

	const std::vector<CommandType> AllCommands = {
		CommandType::Play,
		CommandType::Pause,
		CommandType::Stop,
		CommandType::Load,
		CommandType::Clear,
		CommandType::Seek,
		CommandType::SetVolume,
		CommandType::SetSpeed,
		CommandType::AnalyseFrame,
		CommandType::Status,
		CommandType::Shutdown
	};

	// Rows are commands in AllCommands order, columns are states in AllStates order:
	//                     Empty  Loading  Ready  Playing  Paused
	const bool Legal[11][5] = {
		/* Play         */ { false, false,  true,  true,    true  },
		/* Pause        */ { false, false,  false, true,    true  },
		/* Stop         */ { false, false,  true,  true,    true  },
		/* Load         */ { true,  false,  true,  true,    true  },
		/* Clear        */ { false, false,  true,  true,    true  },
		/* Seek         */ { false, false,  true,  true,    true  },
		/* SetVolume    */ { true,  true,   true,  true,    true  },
		/* SetSpeed     */ { false, false,  true,  true,    true  },
		/* AnalyseFrame */ { false, false,  true,  true,    true  },
		/* Status       */ { true,  true,   true,  true,    true  },
		/* Shutdown     */ { true,  true,   true,  true,    true  }
	};
}

TEST_CASE("Every state x command pair has a defined legality")
{
	for (size_t c = 0; c < AllCommands.size(); ++c)
	{
		for (size_t s = 0; s < AllStates.size(); ++s)
		{
			CAPTURE(Adagio::ToString(AllCommands[c]));
			CAPTURE(Adagio::ToString(AllStates[s]));
			CHECK(Adagio::IsCommandLegal(AllStates[s], AllCommands[c]) == Legal[c][s]);
		}
	}
}

TEST_CASE("Play and Pause are idempotent")
{
	// A double-click used to send two Play commands and terminate the process.
	CHECK(Adagio::IsCommandLegal(TransportState::Playing, CommandType::Play));
	CHECK(Adagio::NextState(TransportState::Playing, CommandType::Play) == TransportState::Playing);

	CHECK(Adagio::IsCommandLegal(TransportState::Paused, CommandType::Pause));
	CHECK(Adagio::NextState(TransportState::Paused, CommandType::Pause) == TransportState::Paused);

	// Stop from Ready is already stopped, and says so rather than erroring.
	CHECK(Adagio::IsCommandLegal(TransportState::Ready, CommandType::Stop));
	CHECK(Adagio::NextState(TransportState::Ready, CommandType::Stop) == TransportState::Ready);
}

TEST_CASE("Load is accepted over a loaded file but refused mid-load")
{
	for (TransportState state : { TransportState::Ready, TransportState::Playing, TransportState::Paused })
	{
		CAPTURE(Adagio::ToString(state));
		CHECK(Adagio::IsCommandLegal(state, CommandType::Load));
		CHECK(Adagio::NextState(state, CommandType::Load) == TransportState::Loading);
	}

	CHECK_FALSE(Adagio::IsCommandLegal(TransportState::Loading, CommandType::Load));
	CHECK(std::string(Adagio::RejectionReason(TransportState::Loading, CommandType::Load)) == "A file is already loading.");
}

TEST_CASE("Commands that need a file are refused when none is loaded")
{
	// Play, Pause, Stop and Clear used to dereference an empty shared_ptr here.
	for (CommandType command : { CommandType::Play, CommandType::Pause, CommandType::Stop,
								 CommandType::Clear, CommandType::Seek, CommandType::SetSpeed,
								 CommandType::AnalyseFrame })
	{
		CAPTURE(Adagio::ToString(command));
		CHECK_FALSE(Adagio::IsCommandLegal(TransportState::Empty, command));
	}
	CHECK(std::string(Adagio::RejectionReason(TransportState::Empty, CommandType::Play)) == "No audio file is loaded.");
}

TEST_CASE("Only the transport commands move the transport")
{
	for (TransportState state : AllStates)
	{
		for (CommandType command : { CommandType::Seek, CommandType::SetVolume, CommandType::SetSpeed,
									 CommandType::AnalyseFrame, CommandType::Status, CommandType::Shutdown })
		{
			CAPTURE(Adagio::ToString(state));
			CAPTURE(Adagio::ToString(command));
			CHECK(Adagio::NextState(state, command) == state);
		}
	}
}

TEST_CASE("Every state and command has a printable name")
{
	for (TransportState state : AllStates)
		CHECK(std::string(Adagio::ToString(state)) != "unknown");
	for (CommandType command : AllCommands)
		CHECK(std::string(Adagio::ToString(command)) != "unknown");
}
