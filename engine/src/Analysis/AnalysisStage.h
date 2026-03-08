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
		virtual void Execute(AnalysisContext* context)
		{
			m_SettingsDefinition = GetSettings();
		}
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

		template <typename T>
		T GetSetting(nlohmann::json settings, const std::string& key) const
		{
			if (m_SettingsDefinition.contains(key))
			{
				const auto& settingDef = m_SettingsDefinition.at(key);
				return settings.value(key, settingDef.value("default", T()));
			}
			return T();
		}

	private:
		nlohmann::json m_SettingsDefinition;
	};
}
