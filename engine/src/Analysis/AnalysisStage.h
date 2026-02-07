#pragma once

namespace Adagio
{
	struct AnalysisContext;

	class AnalysisStage
	{
	public:
		virtual ~AnalysisStage() = default;
		virtual void Execute(AnalysisContext* context) const = 0;
	};
}
