#pragma once
#include "AnalysisStage.h"
#include "AnalysisPipeline.h"

#include <algorithm>
#include <cmath>

namespace Adagio
{
	struct PresenceFilterSettings
	{
		float PRESENCE_WINDOW = 0.2f;
		float OCTAVE_HOLD = 0.35f;
		std::string MASK_ENABLED = "No";
		int MASK_HALF_WIDTH = 4;

		NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(PresenceFilterSettings, PRESENCE_WINDOW, OCTAVE_HOLD, MASK_ENABLED, MASK_HALF_WIDTH)
	};

	class PresenceFilter : public ConfigurableStage<PresenceFilterSettings>
	{
	private:
		static constexpr float ON_LEVEL = 0.6f;
		static constexpr float OFF_LEVEL = 0.25f;
		static constexpr float MATCH_CENTS = 50.0f;
		static constexpr float FORGET_LEVEL = 0.01f;

	public:
		static constexpr std::string_view Name = "PresenceFilter";
		std::string_view GetStageName() const override { return Name; }

		virtual void Execute(AnalysisContext* context) override
		{
			PersistentData& persistent = *context->PersistentData;
			std::vector<TrackedNote>& tracked = persistent.TrackedNotes;
			const double elapsed = Elapsed(context);
			const float keep = KeepFactor(elapsed);

			std::vector<bool> seen(tracked.size(), false);
			for (const Peak& peak : context->Peaks)
			{
				const int match = FindTracked(tracked, peak.Frequency);
				if (match == -1)
				{
					tracked.push_back({ peak, 0.0f, false });
					seen.push_back(true);
				}
				else
				{
					tracked[match].LastPeak = peak;
					tracked[match].OctaveHeld = 0.0f;
					seen[match] = true;
				}
			}
			if (elapsed > 0.0)
				HoldUnderOctaves(tracked, seen, (float)elapsed);

			// A note missing from this frame keeps its last peak while it stays on.
			std::vector<Peak> present;
			for (size_t i = 0; i < tracked.size(); i++)
			{
				TrackedNote& note = tracked[i];
				note.Presence = keep * note.Presence + (1.0f - keep) * (seen[i] ? 1.0f : 0.0f);
				if (note.Presence >= ON_LEVEL)
				{
					note.On = true;
					present.push_back(note.LastPeak);
				}
				else if (note.Presence < OFF_LEVEL)
					note.On = false;
			}
			std::erase_if(tracked, [](const TrackedNote& note) { return !note.On && note.Presence < FORGET_LEVEL; });
			persistent.TrackedTimestamp = context->Frame.Timestamp;

			std::sort(present.begin(), present.end(), [](const Peak& a, const Peak& b)
			{
				return a.Frequency < b.Frequency;
			});

			if (m_Settings.MASK_ENABLED == "Yes")
			{
				kfr::univector<float> maskedMagnitudes(context->Magnitudes.size(), 0.0f);
				for (const Peak& peak : present)
				{
					const int binIndex = (int)std::round(peak.Frequency / context->BinHz);
					const int minBin = std::max(0, binIndex - m_Settings.MASK_HALF_WIDTH);
					const int maxBin = std::min((int)maskedMagnitudes.size() - 1, binIndex + m_Settings.MASK_HALF_WIDTH);
					for (int j = minBin; j <= maxBin; ++j)
						maskedMagnitudes[j] = context->Magnitudes[j];
				}
				context->Magnitudes = std::move(maskedMagnitudes);
			}
			context->Peaks = std::move(present);
		}

		virtual AnalysisStageType GetType() const override
		{
			return AnalysisStageType::Processor;
		}

	protected:
		virtual nlohmann::json BuildSettingsSchema() const override
		{
			return nlohmann::json::parse(R"({
				"PRESENCE_WINDOW": {
					"name": "Presence Window",
					"type": "float",
					"min": 0.0,
					"max": 1.0
				},
				"OCTAVE_HOLD": {
					"name": "Octave Hold",
					"type": "float",
					"min": 0.0,
					"max": 1.0
				},
				"MASK_ENABLED": {
					"name": "Mask Enabled",
					"type": "enum",
					"options": ["No", "Yes"]
				},
				"MASK_HALF_WIDTH": {
					"name": "Mask Half Width",
					"type": "int",
					"min": 1,
					"max": 10
				}
			})");
		}

	private:
		static double Elapsed(const AnalysisContext* context)
		{
			const double elapsed = std::abs(context->Frame.Timestamp - context->PersistentData->TrackedTimestamp);
			const double frameSeconds = context->Frame.FrameLength / (double)context->Frame.SampleRate;
			return elapsed < frameSeconds ? elapsed : 0.0;
		}

		// The share of each note's presence carried over from the last frame: e^(−t/τ) with τ = PRESENCE_WINDOW / 3, as the
		// key's window, over the source time between the frames.
		float KeepFactor(double elapsed) const
		{
			if (m_Settings.PRESENCE_WINDOW <= 0.0f || elapsed == 0.0)
				return 0.0f;
			return (float)std::exp(-elapsed / (m_Settings.PRESENCE_WINDOW / 3.0));
		}

		void HoldUnderOctaves(std::vector<TrackedNote>& tracked, std::vector<bool>& seen, float elapsed) const
		{
			const std::vector<bool> matched = seen;
			for (size_t i = 0; i < tracked.size(); i++)
			{
				TrackedNote& note = tracked[i];
				if (matched[i] || !note.On || note.OctaveHeld >= m_Settings.OCTAVE_HOLD)
					continue;
				for (size_t j = 0; j < tracked.size(); j++)
				{
					const float cents = std::abs(1200.0f * std::log2(tracked[j].LastPeak.Frequency / (2.0f * note.LastPeak.Frequency)));
					if (matched[j] && tracked[j].On && cents < MATCH_CENTS)
					{
						seen[i] = true;
						note.OctaveHeld += elapsed;
						break;
					}
				}
			}
		}

		static int FindTracked(const std::vector<TrackedNote>& tracked, float frequency)
		{
			int match = -1;
			float closest = MATCH_CENTS;
			for (size_t i = 0; i < tracked.size(); i++)
			{
				const float cents = std::abs(1200.0f * std::log2(frequency / tracked[i].LastPeak.Frequency));
				if (cents < closest)
				{
					closest = cents;
					match = (int)i;
				}
			}
			return match;
		}
	};
}
