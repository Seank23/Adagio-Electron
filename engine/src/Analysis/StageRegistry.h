#pragma once
#include <memory>
#include <string_view>

namespace Adagio
{
	class AnalysisStage;

	// Null for a name it doesn't know.
	std::unique_ptr<AnalysisStage> CreateStage(std::string_view name);
	bool IsKnownStage(std::string_view name);
}
