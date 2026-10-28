#include "StageRegistry.h"
#include "Stages/ChordPredictor.h"
#include "Stages/FFTProcessor.h"
#include "Stages/HPSDownsamplerProcessor.h"
#include "Stages/HarmonicGrouper.h"
#include "Stages/KeyDetector.h"
#include "Stages/NoteDetector.h"
#include "Stages/NoteSelector.h"
#include "Stages/PartialExtractor.h"
#include "Stages/PeakExtractor.h"
#include "Stages/PresenceFilter.h"
#include "Stages/SpectrumFilterProcessor.h"

#include <algorithm>
#include <utility>

namespace Adagio
{
	namespace
	{
		template<typename TStage>
		std::unique_ptr<AnalysisStage> MakeStage()
		{
			return std::make_unique<TStage>();
		}

		constexpr std::pair<std::string_view, std::unique_ptr<AnalysisStage>(*)()> StageFactories[] = {
			{ FFTProcessor::Name, &MakeStage<FFTProcessor> },
			{ SpectrumFilterProcessor::Name, &MakeStage<SpectrumFilterProcessor> },
			{ HPSDownsamplerProcessor::Name, &MakeStage<HPSDownsamplerProcessor> },
			{ PeakExtractor::Name, &MakeStage<PeakExtractor> },
			{ PartialExtractor::Name, &MakeStage<PartialExtractor> },
			{ HarmonicGrouper::Name, &MakeStage<HarmonicGrouper> },
			{ NoteSelector::Name, &MakeStage<NoteSelector> },
			{ PresenceFilter::Name, &MakeStage<PresenceFilter> },
			{ NoteDetector::Name, &MakeStage<NoteDetector> },
			{ KeyDetector::Name, &MakeStage<KeyDetector> },
			{ ChordPredictor::Name, &MakeStage<ChordPredictor> }
		};
	}

	std::unique_ptr<AnalysisStage> CreateStage(std::string_view name)
	{
		for (const auto& [stageName, create] : StageFactories)
		{
			if (stageName == name)
				return create();
		}
		return nullptr;
	}

	bool IsKnownStage(std::string_view name)
	{
		return std::ranges::any_of(StageFactories, [name](const auto& factory) { return factory.first == name; });
	}
}
