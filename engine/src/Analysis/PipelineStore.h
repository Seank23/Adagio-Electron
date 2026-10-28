#pragma once
#include <nlohmann/json.hpp>
#include <chrono>
#include <filesystem>
#include <optional>
#include <vector>
#include <string>

namespace Adagio
{
	struct StageEntry
	{
		std::string StageID;
		nlohmann::json Settings;
	};

	struct PipelineDefinition
	{
		std::string ID;
		std::string Name;
		std::string Description;
		bool BuiltIn = false;
		std::vector<StageEntry> Stages;
	};

	class PipelineStore
	{
	private:
		static constexpr std::chrono::milliseconds SAVE_DELAY_MS{ 500 };
		static constexpr int FILE_VERSION = 1;

	public:
		explicit PipelineStore(const std::filesystem::path& path);

		std::vector<std::string> Load();
		const PipelineDefinition* Find(const std::string& id) const;

		void Save();
		void SaveIfDue(std::chrono::steady_clock::time_point now);

		const std::string& GetActiveId() const;
		void SetActiveId(const std::string& id);
		void SetValues(const std::string& id, const nlohmann::json& values);

		nlohmann::json SummaryJson() const;

	private:
		const std::vector<PipelineDefinition>& BuiltInPipelines();

		std::vector<PipelineDefinition> m_Pipelines;
		std::string m_ActivePipelineId;
		std::filesystem::path m_FilePath;
		bool m_IsDirty = false;
		std::optional<std::chrono::steady_clock::time_point> m_SaveDueTime;
	};
}