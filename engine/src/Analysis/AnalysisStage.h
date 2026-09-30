#pragma once
#include <nlohmann/json.hpp>

#include <string>
#include <string_view>

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

		void Initialise()
		{
			m_Name = std::string(GetStageName());

			m_SettingsDefinition = BuildSettingsSchema();
			const nlohmann::json defaults = GetDefaultValues();
			for (auto& definition : m_SettingsDefinition.items())
			{
				if (defaults.contains(definition.key()))
					definition.value()["default"] = defaults.at(definition.key());
			}
		}

		virtual void Execute(AnalysisContext* context) = 0;

		virtual std::string_view GetStageName() const = 0;
		virtual AnalysisStageType GetType() const = 0;

		virtual void ApplySettings(const nlohmann::json& values) {}

		const nlohmann::json& GetSettingsSchema() const { return m_SettingsDefinition; }
		const std::string& GetName() const { return m_Name; }

	protected:
		virtual nlohmann::json BuildSettingsSchema() const { return nlohmann::json::object(); }
		virtual nlohmann::json GetDefaultValues() const { return nlohmann::json::object(); }

	private:
		std::string m_Name;
		nlohmann::json m_SettingsDefinition;
	};

	template <typename TSettings>
	class ConfigurableStage : public AnalysisStage
	{
	public:
		virtual void ApplySettings(const nlohmann::json& values) override
		{
			m_Settings = values.get<TSettings>();
		}

	protected:
		virtual nlohmann::json GetDefaultValues() const override
		{
			return TSettings{};
		}

		TSettings m_Settings;
	};
}
