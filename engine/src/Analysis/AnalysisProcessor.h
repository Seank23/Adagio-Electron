#pragma once

namespace Adagio
{
	class AnalysisProcessor
	{
	public:
		virtual ~AnalysisProcessor() = default;
		virtual void* Execute(void* data, void* args) const = 0;
	};
}
