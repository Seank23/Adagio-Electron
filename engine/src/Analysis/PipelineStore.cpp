#include "PipelineStore.h"
#include "StageRegistry.h"
#include "Stages/ChordPredictor.h"
#include "Stages/FFTProcessor.h"
#include "Stages/HPSDownsamplerProcessor.h"
#include "Stages/KeyDetector.h"
#include "Stages/NoteDetector.h"
#include "Stages/PeakExtractor.h"
#include "Stages/SpectrumFilterProcessor.h"
#include "Stages/PartialExtractor.h"
#include "Stages/HarmonicGrouper.h"
#include "Stages/NoteSelector.h"
#include "Stages/PresenceFilter.h"
#include "../Core/MessageQueue.h"
#include "Protocol.generated.h"

#include <algorithm>
#include <fstream>
#include <set>

namespace Adagio
{
	namespace
	{
		bool ReadPipeline(const nlohmann::json& entry, PipelineDefinition& outPipeline, std::string& outError)
		{
			const auto id = entry.find("id");
			if (!entry.is_object() || id == entry.end() || !id->is_string() || id->get<std::string>().empty())
			{
				outError = "a pipeline has no id";
				return false;
			}
			outPipeline.ID = id->get<std::string>();

			const auto name = entry.find("name");
			if (name != entry.end() && name->is_string())
				outPipeline.Name = name->get<std::string>();
			const auto description = entry.find("description");
			if (description != entry.end() && description->is_string())
				outPipeline.Description = description->get<std::string>();

			const auto stages = entry.find("stages");
			if (stages == entry.end() || !stages->is_array())
			{
				outError = "pipeline '" + outPipeline.ID + "' has no list of stages";
				return false;
			}

			for (const nlohmann::json& stage : *stages)
			{
				const auto stageName = stage.find("stage");
				if (!stage.is_object() || stageName == stage.end() || !stageName->is_string())
				{
					outError = "pipeline '" + outPipeline.ID + "' has a stage with no name";
					return false;
				}

				StageEntry stageEntry{ stageName->get<std::string>(), nlohmann::json::object() };
				const auto settings = stage.find("settings");
				if (settings != stage.end())
				{
					if (!settings->is_object())
					{
						outError = "pipeline '" + outPipeline.ID + "' has settings for " + stageEntry.StageID + " that aren't an object";
						return false;
					}
					stageEntry.Settings = *settings;
				}

				const bool listed = std::ranges::any_of(outPipeline.Stages, [&stageEntry](const StageEntry& other) { return other.StageID == stageEntry.StageID; });
				if (listed)
				{
					outError = "pipeline '" + outPipeline.ID + "' lists " + stageEntry.StageID + " twice";
					return false;
				}
				outPipeline.Stages.push_back(std::move(stageEntry));
			}
			return true;
		}

		// A built-in in the file only gives the values of the stages it still has; any other id is a custom pipeline,
		// appended as written. Fails on the first problem, since a pipeline dropped here would be gone at the next save.
		bool ReadFile(const std::filesystem::path& path, std::vector<PipelineDefinition>& inOutPipelines, std::string& outActiveId, std::string& outError)
		{
			std::ifstream file(path, std::ios::binary);
			if (!file)
			{
				outError = "it couldn't be opened";
				return false;
			}

			const nlohmann::json root = nlohmann::json::parse(file, nullptr, false);
			if (root.is_discarded())
			{
				outError = "not JSON";
				return false;
			}

			const auto active = root.find("active");
			const auto pipelines = root.find("pipelines");
			if (!root.is_object() || active == root.end() || !active->is_string() || pipelines == root.end() || !pipelines->is_array())
			{
				outError = "not in the correct format: { active, pipelines: [...] }";
				return false;
			}

			std::set<std::string> ids;
			for (const nlohmann::json& entry : *pipelines)
			{
				PipelineDefinition stored;
				if (!ReadPipeline(entry, stored, outError))
					return false;
				if (!ids.insert(stored.ID).second)
				{
					outError = "the id '" + stored.ID + "' is used twice";
					return false;
				}

				const auto builtIn = std::ranges::find_if(inOutPipelines, [&stored](const PipelineDefinition& pipeline) { return pipeline.BuiltIn && pipeline.ID == stored.ID; });
				if (builtIn != inOutPipelines.end())
				{
					for (StageEntry& stage : builtIn->Stages)
					{
						const auto value = std::ranges::find_if(stored.Stages, [&stage](const StageEntry& other) { return other.StageID == stage.StageID; });
						if (value != stored.Stages.end())
							stage.Settings = std::move(value->Settings);
					}
					continue;
				}

				if (stored.Name.empty())
				{
					outError = "pipeline '" + stored.ID + "' has no name";
					return false;
				}
				if (stored.Stages.empty())
				{
					outError = "pipeline '" + stored.ID + "' has no stages";
					return false;
				}
				for (const StageEntry& stage : stored.Stages)
				{
					if (!IsKnownStage(stage.StageID))
					{
						outError = "pipeline '" + stored.ID + "' has an unknown stage: " + stage.StageID;
						return false;
					}
				}
				inOutPipelines.push_back(std::move(stored));
			}

			outActiveId = active->get<std::string>();
			return true;
		}

		std::string SetAside(const std::filesystem::path& path, const std::string& problem)
		{
			std::filesystem::path aside = path;
			aside += ".bad";
			std::error_code error;
			std::filesystem::copy_file(path, aside, std::filesystem::copy_options::overwrite_existing, error);

			const std::string kept = error ? "Couldn't keep a copy: " + error.message() : "Kept a copy as " + aside.filename().string() + ".";
			return "Ignoring " + path.filename().string() + ": " + problem + ". " + kept;
		}

		// Written aside and renamed over the file, so a crash mid-write can't leave half a file.
		bool WriteFile(const std::filesystem::path& path, const std::string& text, std::string& outError)
		{
			std::filesystem::path temporary = path;
			temporary += ".tmp";

			std::ofstream file(temporary, std::ios::binary);
			file << text;
			file.close();

			std::error_code error;
			if (!file)
			{
				std::filesystem::remove(temporary, error);
				outError = temporary.filename().string() + " couldn't be written";
				return false;
			}

			std::filesystem::rename(temporary, path, error);
			if (error)
			{
				outError = error.message();
				std::filesystem::remove(temporary, error);
				return false;
			}
			return true;
		}
	}

	PipelineStore::PipelineStore(const std::filesystem::path& path)
		: m_FilePath(path)
	{}

	std::vector<std::string> PipelineStore::Load()
	{
		const auto& builtin = BuiltInPipelines();
		m_Pipelines = builtin;
		m_ActivePipelineId = builtin.front().ID;
		m_IsDirty = true;

		std::error_code existsError;
		if (m_FilePath.empty() || (!std::filesystem::exists(m_FilePath, existsError) && !existsError))
			return {};

		std::vector<PipelineDefinition> pipelines = builtin;
		std::string activeId;
		std::string error;
		if (!ReadFile(m_FilePath, pipelines, activeId, error))
			return { SetAside(m_FilePath, error) };

		m_Pipelines = std::move(pipelines);
		if (Find(activeId))
		{
			m_ActivePipelineId = activeId;
			m_IsDirty = false;
		}
		return {};
	}

	const PipelineDefinition* PipelineStore::Find(const std::string& id) const
	{
		const auto pipeline = std::ranges::find_if(m_Pipelines, [&id](const PipelineDefinition& definition) { return definition.ID == id; });
		return pipeline != m_Pipelines.end() ? &*pipeline : nullptr;
	}

	void PipelineStore::Save()
	{
		if (!m_IsDirty)
			return;

		m_IsDirty = false;
		m_SaveDueTime.reset();
		if (m_FilePath.empty())
			return;

		nlohmann::json pipelines = nlohmann::json::array();
		for (const PipelineDefinition& pipeline : m_Pipelines)
		{
			nlohmann::json stages = nlohmann::json::array();
			for (const StageEntry& stage : pipeline.Stages)
				stages.push_back({ {"stage", stage.StageID}, {"settings", stage.Settings} });

			pipelines.push_back({
				{ "id", pipeline.ID },
				{ "name", pipeline.Name },
				{ "description", pipeline.Description },
				{ "builtIn", pipeline.BuiltIn },
				{ "stages", std::move(stages) },
			});
		}
		const nlohmann::json root = {
			{ "version", FILE_VERSION },
			{ "active", m_ActivePipelineId },
			{ "pipelines", std::move(pipelines) },
		};

		std::string error;
		if (!WriteFile(m_FilePath, root.dump(4, ' ', false, nlohmann::json::error_handler_t::replace), error))
			MessageQueue::GetInstance().Push(nlohmann::json{ {"type", Protocol::Event::Error}, {"value", "Couldn't save the analysis settings: " + error} }.dump());
	}

	void PipelineStore::SaveIfDue(std::chrono::steady_clock::time_point now)
	{
		if (!m_IsDirty)
			return;

		// A change clears the due time, so it's counted from the last change: a slider's whole drag is one write.
		if (!m_SaveDueTime)
		{
			m_SaveDueTime = now + SAVE_DELAY_MS;
			return;
		}
		if (now >= *m_SaveDueTime)
			Save();
	}

	const std::string& PipelineStore::GetActiveId() const
	{
		return m_ActivePipelineId;
	}

	void PipelineStore::SetActiveId(const std::string& id)
	{
		m_ActivePipelineId = id;
		m_IsDirty = true;
		m_SaveDueTime.reset();
	}

	void PipelineStore::SetValues(const std::string& id, const nlohmann::json& values)
	{
		if (!values.is_object())
			return;
		if (PipelineDefinition* pipeline = const_cast<PipelineDefinition*>(Find(id)))
		{
			for (StageEntry& stage : pipeline->Stages)
			{
				const auto value = values.find(stage.StageID);
				if (value != values.end())
					stage.Settings = *value;
			}
			m_IsDirty = true;
			m_SaveDueTime.reset();
		}
	}

	nlohmann::json PipelineStore::SummaryJson() const
	{
		const PipelineDefinition* activePipeline = Find(m_ActivePipelineId);
		return {
			{ "id", m_ActivePipelineId },
			{ "name", activePipeline ? activePipeline->Name : "" },
			{ "description", activePipeline ? activePipeline->Description : "" },
		};
	}

	const std::vector<PipelineDefinition>& PipelineStore::BuiltInPipelines()
	{
		static const std::vector<PipelineDefinition> builtinPipelines = {
			PipelineDefinition{
				.ID = "hps",
				.Name = "Harmonic Product Spectrum",
				.Description = "The original pipeline: multiplies downsampled spectra so fundamentals stand out, then picks peaks.",
				.BuiltIn = true,
				.Stages = {
					StageEntry{ std::string(FFTProcessor::Name), nlohmann::json::object() },
					StageEntry{ std::string(SpectrumFilterProcessor::Name), nlohmann::json::object() },
					StageEntry{ std::string(HPSDownsamplerProcessor::Name), nlohmann::json::object() },
					StageEntry{ std::string(PeakExtractor::Name), nlohmann::json::object() },
					StageEntry{ std::string(PresenceFilter::Name), nlohmann::json::object() },
					StageEntry{ std::string(NoteDetector::Name), nlohmann::json::object() },
					StageEntry{ std::string(KeyDetector::Name), nlohmann::json::object() },
					StageEntry{ std::string(ChordPredictor::Name), nlohmann::json::object() }
				}
			},
			PipelineDefinition{
				.ID = "partialTracking",
				.Name = "Partial Tracking",
				.Description = "Finds partials, groups them into harmonic series and picks notes by salience.",
				.BuiltIn = true,
				.Stages = {
					StageEntry{ std::string(FFTProcessor::Name), nlohmann::json::object() },
					StageEntry{ std::string(SpectrumFilterProcessor::Name), nlohmann::json::object() },
					StageEntry{ std::string(PartialExtractor::Name), nlohmann::json::object() },
					StageEntry{ std::string(HarmonicGrouper::Name), nlohmann::json::object() },
					StageEntry{ std::string(NoteSelector::Name), nlohmann::json::object() },
					StageEntry{ std::string(PresenceFilter::Name), nlohmann::json::object() },
					StageEntry{ std::string(NoteDetector::Name), nlohmann::json::object() },
					StageEntry{ std::string(KeyDetector::Name), nlohmann::json::object() },
					StageEntry{ std::string(ChordPredictor::Name), nlohmann::json::object() }
				}
			}
		};
		return builtinPipelines;
	}
}