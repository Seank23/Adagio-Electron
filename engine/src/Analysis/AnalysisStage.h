#pragma once

namespace Adagio
{
	struct AnalysisContext;

	enum class AnalysisStageType
	{
		Processor,
		FeatureExtractor,
	};

	class AnalysisStage
	{
	public:
		virtual ~AnalysisStage() = default;
		virtual void Execute(AnalysisContext* context) const = 0;
		virtual AnalysisStageType GetType() const = 0;
	};
}
