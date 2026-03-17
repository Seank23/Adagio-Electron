#pragma once
#include "AnalysisStage.h"
#include "AnalysisPipeline.h"
#include "AnalysisUtils.h"
#include <iostream>
#include <algorithm>
#include <string>

namespace Adagio
{
	class ChordPredictor : public AnalysisStage
	{
	public:
		virtual void Execute(AnalysisContext* context) override
		{
			AnalysisStage::Execute(context);
			auto& data = context->Peaks;
			nlohmann::json settings = context->Settings;

			const float rollingWindowTime = GetSetting<float>(settings, "ROLLING_WINDOW");
			const double timestamp = context->Frame.Timestamp;

			if (context->PersistentData->RollingNotes.empty())
				return;

			std::vector<Note> rollingNotes;
			for (size_t i = context->PersistentData->RollingNotes.size() - 1; i > 0; i--)
			{
				if (timestamp - context->PersistentData->RollingNotes[i].Timestamp <= rollingWindowTime)
					rollingNotes.push_back(context->PersistentData->RollingNotes[i]);
				else
					break;
			}

			std::map<int, float> frequencyHistogram;
			std::array<double, 12> notesHistogram = { 0.0 };
			std::map<int, std::vector<Note>> noteClasses;
			for (const auto& note : rollingNotes)
			{
				int roundedFrequency = std::round(note.PeakInfo.Frequency);
				frequencyHistogram[roundedFrequency] += note.PeakInfo.Score;

				int noteClass = note.Midi % 12;
				notesHistogram[noteClass] += note.PeakInfo.Score;
				noteClasses[noteClass].push_back(note);
			}

			float sum = std::reduce(notesHistogram.begin(), notesHistogram.end(), 0.0f, std::plus<float>());
			if (sum > 0)
			{
				for (auto& val : notesHistogram)
					val /= sum;
			}

			std::vector<std::pair<int, float>> chordNotes;
			for (int i = 0; i < notesHistogram.size(); i++)
			{
				if (notesHistogram[i] >= GetSetting<float>(settings, "SCORE_THRESHOLD"))
					chordNotes.push_back({ i, notesHistogram[i] });

                std::sort(noteClasses[i].begin(), noteClasses[i].end(), [](const Note& a, const Note& b)
                {
                    return a.PeakInfo.Score > b.PeakInfo.Score;
                });
			}

			std::vector<int> intervals;
			std::vector<Note> prominentNotes;
			std::vector<Chord> possibleChords;
			for (int i = 0; i < chordNotes.size(); i++)
			{
				intervals.clear();
				prominentNotes.clear();
				for (int j = 1; j < chordNotes.size(); j++)
				{
					int noteDiff = chordNotes[j].first - chordNotes[0].first;
                    if (noteDiff < 0)
						noteDiff += 12;
					intervals.push_back(noteDiff);
				}
                for (int j = 0; j < chordNotes.size(); j++)
                {
                    int noteClass = chordNotes[j].first;
                    if (!noteClasses[noteClass].empty())
						prominentNotes.push_back(noteClasses[noteClass][0]);
                }
				int fifthOmitted = 0;
				std::string chordQuality = GetChordQuality(intervals, fifthOmitted);
                if (chordQuality != "N/A")
                {
                    Chord chord;
                    chord.Root = NoteNames.at(chordNotes[0].first);
                    chord.Quality = chordQuality;
					chord.Name = chord.Root + chord.Quality;
                    chord.Notes = prominentNotes;
					chord.RootOccurences = noteClasses[chordNotes[0].first].size();
                    chord.NumExtentions = fifthOmitted == 1 ? intervals.size() - 2 : intervals.size() - 3;
                    chord.FifthOmitted = fifthOmitted;
                    possibleChords.push_back(chord);
				}
                std::rotate(chordNotes.begin(), chordNotes.begin() + 1, chordNotes.end());
			}
			CalculateProbabilities(possibleChords, context);
            std::sort(possibleChords.begin(), possibleChords.end(), [](const Chord& a, const Chord& b)
            {
                return a.Probability > b.Probability;
            });

			context->PersistentData->PreviousChord = possibleChords.empty() ? Chord() : possibleChords[0];
			context->ChordFrequencyHistogram = std::move(frequencyHistogram);
			context->PredictedChords = std::move(possibleChords);
		}

		virtual AnalysisStageType GetType() const override
		{
			return AnalysisStageType::FeatureExtractor;
		}

		virtual nlohmann::json GetSettings() const override
		{
			return nlohmann::json::parse(R"({
				"ROLLING_WINDOW": {
					"name": "Rolling Window",
					"type": "float",	
					"min": 0.01,
					"max": 5.0,	
					"default": 0.5
				},
				"SCORE_THRESHOLD": {
					"name": "Score Threshold",
					"type": "float",	
					"min": 0.0,
					"max": 1.0,	
					"default": 0.05
				}
			})");
		}

	private:
        bool Contains(const std::vector<int>& vec, int value)
        {
            return std::find(vec.begin(), vec.end(), value) != vec.end();
		}

        void Remove(std::vector<int>& vec, int value)
        {
            vec.erase(std::remove(vec.begin(), vec.end(), value), vec.end());
		}

		std::string GetChordQuality(std::vector<int> intervals, int& outFifthOmitted)
		{
            outFifthOmitted = 0;
            while (outFifthOmitted <= 1)
            {
                if (Contains(intervals, 4) && Contains(intervals, 7)) // Major chords
                {
                    Remove(intervals, 4);
                    Remove(intervals, 7);

                    if (Contains(intervals, 2)) // Contains 2/9
                    {
                        Remove(intervals, 2);
                        if (Contains(intervals, 9)) // Contains 6
                        {
                            Remove(intervals, 9);
                            return "6/9" + AddRemainingNotes(intervals);
                        }
                        if (Contains(intervals, 10)) // Contains m7
                        {
                            Remove(intervals, 10);
                            if (Contains(intervals, 5))
                            {
                                Remove(intervals, 5);
                                return "11" + AddRemainingNotes(intervals);
                            }
                            return "9" + AddRemainingNotes(intervals);
                        }
                        if (Contains(intervals, 11)) // Contains maj7
                        {
                            Remove(intervals, 11);
                            if (Contains(intervals, 9)) // Contains maj6
                            {
                                Remove(intervals, 9);
                                return "maj13" + AddRemainingNotes(intervals);
                            }
                            return "maj9" + AddRemainingNotes(intervals);
                        }
                        return "add9" + AddRemainingNotes(intervals);
                    }

                    if (Contains(intervals, 9)) // Contains maj6
                    {
                        Remove(intervals, 9);
                        return "6" + AddRemainingNotes(intervals);
                    }

                    if (Contains(intervals, 10)) // Contains m7
                    {
                        Remove(intervals, 10);
                        if (Contains(intervals, 1)) // Contains b2/9
                        {
                            Remove(intervals, 1);
                            return "7b9" + AddRemainingNotes(intervals);
                        }

                        if (Contains(intervals, 3)) // Contains #2/9
                        {
                            Remove(intervals, 3);
                            return "7#9" + AddRemainingNotes(intervals);
                        }
                        return "7" + AddRemainingNotes(intervals);
                    }

                    if (Contains(intervals, 11)) // Contains maj7
                    {
                        Remove(intervals, 11);
                        return "maj7" + AddRemainingNotes(intervals);
                    }
                    return "" + AddRemainingNotes(intervals);
                }

                if (Contains(intervals, 3) && Contains(intervals, 7)) // Minor chords
                {
                    Remove(intervals, 3);
                    Remove(intervals, 7);

                    if (Contains(intervals, 9)) // Contains 6
                    {
                        Remove(intervals, 9);
                        return "m6" + AddRemainingNotes(intervals);
                    }

                    if (Contains(intervals, 10)) // Contains m7
                    {
                        Remove(intervals, 10);
                        if (Contains(intervals, 2)) // Contains 2/9
                        {
                            Remove(intervals, 2);
                            return "m9" + AddRemainingNotes(intervals);
                        }
                        return "m7" + AddRemainingNotes(intervals);
                    }

                    if (Contains(intervals, 11)) // Contains maj7
                    {
                        Remove(intervals, 11);
                        return "mM7" + AddRemainingNotes(intervals);
                    }
                    return "m" + AddRemainingNotes(intervals);
                }

                if (Contains(intervals, 2) && Contains(intervals, 7)) // Suspended 2 chords
                {
                    Remove(intervals, 2);
                    Remove(intervals, 7);

                    if (Contains(intervals, 10)) // Contains m7
                    {
                        Remove(intervals, 10);
                        if (Contains(intervals, 2)) // Contains 2/9
                        {
                            Remove(intervals, 2);
                            if (Contains(intervals, 5))
                            {
                                Remove(intervals, 5);
                                return "9sus2" + AddRemainingNotes(intervals);
                            }
                            return "7sus2" + AddRemainingNotes(intervals);
                        }
                    }
                    return "sus2" + AddRemainingNotes(intervals);
                }

                if (Contains(intervals, 5) && Contains(intervals, 7)) // Suspended 4 chords
                {
                    Remove(intervals, 5);
                    Remove(intervals, 7);

                    if (Contains(intervals, 10)) // Contains m7
                    {
                        Remove(intervals, 10);
                        if (Contains(intervals, 2)) // Contains 2/9
                        {
                            Remove(intervals, 2);
                            if (Contains(intervals, 5))
                            {
                                Remove(intervals, 5);
                                return "9sus4" + AddRemainingNotes(intervals);
                            }
                            return "7sus4" + AddRemainingNotes(intervals);
                        }
                    }
                    return "sus4" + AddRemainingNotes(intervals);
                }

                if (Contains(intervals, 3) && Contains(intervals, 6)) // Diminished chords
                {
                    Remove(intervals, 3);
                    Remove(intervals, 6);

                    if (Contains(intervals, 9)) // Contains 6
                    {
                        Remove(intervals, 9);
                        return "dim7" + AddRemainingNotes(intervals);
                    }
                    if (Contains(intervals, 10)) // Contains m7
                    {
                        Remove(intervals, 10);
                        return "m7b5" + AddRemainingNotes(intervals);
                    }
                    return "dim" + AddRemainingNotes(intervals);
                }

                if (Contains(intervals, 4) && Contains(intervals, 8)) // Augmented chords
                {
                    Remove(intervals, 4);
                    Remove(intervals, 8);
                    return "aug" + AddRemainingNotes(intervals);
                }

                if (Contains(intervals, 7) && intervals.size() == 1 && outFifthOmitted == 0) // 5 chord/power chord
                {
                    return "5";
                }

                intervals.push_back(7);
                outFifthOmitted += 1;
            }
            return "N/A";
		}

        std::string AddRemainingNotes(const std::vector<int>& intervals)
        {
            if (intervals.empty())
				return "";

            std::string result = " (";
            for (int interval : intervals)
            {
                if (interval == 1)
                    result += "b9";
                else if (interval == 2)
                    result += "9";
                else if (interval == 3)
                    result += "#9";
                else if (interval == 4)
                    result += "b11";
                else if (interval == 5)
                    result += "11";
                else if (interval == 6)
                    result += "+11";
                else if (interval == 8)
                    result += "#5";
                else if (interval == 9)
                    result += "6";
                else if (interval == 10)
                    result += "m7";
                else if (interval == 11)
                    result += "maj7";
                if (interval != intervals.back())
					result += ", ";
            }
            return result + ")";
		}

        void Normalise(std::vector<float>& vec)
        {
            float maxVal = *std::max_element(vec.begin(), vec.end());
            float minVal = *std::min_element(vec.begin(), vec.end());
            float range = maxVal - minVal;
            if (range > 1e-6f)
            {
                for (auto& val : vec)
                    val = (val - minVal) / range;
            }
		}

        void CalculateProbabilities(std::vector<Chord>& chords, AnalysisContext* context)
        {
            if (chords.empty()) return;

            std::vector<float> rootMagnitudes(chords.size());
            float avgMagnitude = 0.0f;
            for (int i = 0; i < chords.size(); i++)
            {
                rootMagnitudes[i] = chords[i].Notes[0].PeakInfo.Magnitude;
				avgMagnitude += rootMagnitudes[i];
            }
			avgMagnitude /= rootMagnitudes.size();
            for (int i = 0; i < rootMagnitudes.size(); i++)
                rootMagnitudes[i] -= avgMagnitude;
            Normalise(rootMagnitudes);

            std::vector<float> rootOccurences(chords.size());
            float avgOccurences = 0.0f;
            for (int i = 0; i < chords.size(); i++)
            {
                rootOccurences[i] = chords[i].RootOccurences;
                avgOccurences += rootOccurences[i];
            }
            avgOccurences /= rootOccurences.size();
            for (int i = 0; i < rootOccurences.size(); i++)
                rootOccurences[i] -= avgMagnitude;
            Normalise(rootOccurences);

            std::vector<float> rootFreq(chords.size());
            float avgFreq = 0.0f;
            for (int i = 0; i < chords.size(); i++)
            {
                rootFreq[i] = chords[i].Notes[0].PeakInfo.Frequency;
                avgFreq += rootFreq[i];
            }
            avgFreq /= rootFreq.size();
            for (int i = 0; i < rootFreq.size(); i++)
                rootFreq[i] = avgFreq - rootFreq[i];
            Normalise(rootFreq);

            std::vector<float> chordExtensions(chords.size());
            float avgExtensions = 0.0f;
            for (int i = 0; i < chords.size(); i++)
            {
                chordExtensions[i] = chords[i].NumExtentions;
                avgExtensions += chordExtensions[i];
            }
            avgExtensions /= chordExtensions.size();
            for (int i = 0; i < chordExtensions.size(); i++)
                chordExtensions[i] = avgExtensions - chordExtensions[i];
            Normalise(chordExtensions);

			std::vector<float> notSuspended(chords.size());
            for (int i = 0; i < chords.size(); i++)
				notSuspended[i] = chords[i].Quality.find("sus") != std::string::npos ? -1.0f : 1.0f;

			std::vector<float> fifthOmitted(chords.size());
            for (int i = 0; i < chords.size(); i++)
                fifthOmitted[i] = chords[i].FifthOmitted;
			Normalise(fifthOmitted);

            std::vector<float> chordPredictedBefore(chords.size());
            for (int i = 0; i < chords.size(); i++)
				chordPredictedBefore[i] = context->PersistentData->PreviousChord.Root == chords[i].Root ? 1.0f : 0.0f;

            std::vector<float> overallProb(chords.size());
            for (int i = 0; i < chords.size(); i++)
                overallProb[i] = 1.1f * rootMagnitudes[i] + 1.0f * rootOccurences[i] + 2.3f * chordExtensions[i] + 1.8f * rootFreq[i] + 2.5f * notSuspended[i] + 1.0f * fifthOmitted[i] + 0.7f * chordPredictedBefore[i];
            Normalise(overallProb);
            float probSum = overallProb[0] + 1;
            for (int i = 1; i < chords.size(); i++)
                probSum += overallProb[i] + 1;
            for (int i = 0; i < chords.size(); i++)
                chords[i].Probability = (overallProb[i] + 1) / probSum * 100;
        }
	};
}
