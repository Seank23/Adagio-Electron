#pragma once
#include <nlohmann/json.hpp>

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
		virtual nlohmann::json GetSettings() const = 0;

		std::string GetName() const
		{
			std::string name = typeid(*this).name();
			size_t pos = name.find_last_of("::");
			if (pos != std::string::npos)
				name = name.substr(pos + 1);
			return name;
		}
	};
}
