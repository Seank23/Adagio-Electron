# The metric: scoring a run against the transcription

`scripts/metric.py` scores every analysis frame against what the score says is sounding at that moment, then averages
each component over the frames. The total is a weighted sum in [0, 1]. On the first track a perfect run scored 0.99
and an empty run 0. It was designed for the user's brief to "prioritise note matches based on both the melody notes and
chord notes playing at that point". Change it if a different brief calls for it (see the end).

## Components

| Part | Weight | Per frame | Why |
|---|---|---|---|
| melody | 0.30 | Each melody note sounding through all of [t − 0.10, t] is in the frame: exact MIDI 1.0, other octave 0.5 (1.0 in `looseOctaveBars`). Mean over required notes; frames with none are skipped. | The melody is what a listener checks first. The 100 ms hold stops a note being required before the pipeline could have seen it (PresenceFilter alone holds back ~0.2 s windows). Half credit for the octave: the pitch class is right, which is what the key and chords use. |
| harmony | 0.20 | Half the bass note(s) (exact 1.0, other octave 0.5), half the share of the chart chord's pitch classes found. Just the chord share where no bass note is sounding. | The bass anchors the chord; chord-tone coverage rewards hearing the accompaniment, not only the top line. |
| precision | 0.20 | Share of the frame's notes whose pitch class is a tone of a chart chord, or a melody or bass note, of anything sounding within [t − 0.30, t + 0.05]. | Recall alone is maximised by reporting every note. The window allows release tails, the chord window's lag and the frame's centring. Pitch class, not octave: doubling a chord tone in another octave is real in most arrangements. |
| chord | 0.20 | Top predicted chord: 0.5 if its root is the chart chord's (either note of a slash chord) + 0.5 × Jaccard of the two pitch-class sets; best over the chart chords within the same window. | Partial credit separates "Ebmaj7 for Eb" (good) from "Gm for Eb" (shares tones, wrong root) from nonsense. |
| key | 0.10 | The credit `truth.json` gives the detected key's name (tonic 1.0, relative 0.5). | The key is one label for the whole track, so it gets the least weight per frame, but a key that flips mid-song is visible here. |

Frames outside the grid (before bar 1, after the last bar) are ignored. `notesPerFrame` (n/f) is reported alongside.

## Reading the numbers

- **Watch n/f and the notes F1 beside the total.** The total is recall-heavy (melody + harmony = 0.5 against precision's
  0.2), so it can climb by reporting more notes. On the first track the best total had ~12.8 notes a frame against 5-6 in
  the score; a cleaner run (iteration 72, ~7 notes a frame) scored 0.78. Mention the trade-off in the report rather than
  hiding it. `summary.py` prints F1 = harmonic mean of (melody, harmony recall) and precision.
- **Label-only stages leave the note components untouched.** Sweeping KeyDetector or ChordPredictor gives identical
  melody/harmony/precision, so only the chord and key columns move. Use that to check that a run did what you meant.
- **A plateau shows in the components first.** When precision stops rising while melody and harmony trade against it,
  settings have run out: find out what the false notes are (`diagnose.py`) before spending more runs.
- **Per-bar breakdown** (`metric.py runs/NNN.json`): a bar that scores badly under every configuration is more likely a
  transcription or alignment error than a pipeline one.

## Changing it

The frames of every run are saved (`runs/NNN.json`), so the metric, the truth and the grid can all change mid-campaign:
`rescore.py` recomputes every run's score. Change it when the brief differs:
- **Chords only** (a chord-chart tool): raise chord to ~0.5, drop melody.
- **Melody only** (a tuner, a transcription aid): melody ~0.6, precision ~0.3, and score precision against the melody
  alone.
- **Instrumental tracks**: the melody is the lead instrument's line. Leave the octave rule as it is.
- **A slower or faster pipeline** (longer presence windows, a longer chord window): widen `MELODY_HOLD` and `TOLERANCE`
  in `metric.py` to match its latency, or it is penalised for lag it is designed to have.

Keep the weights fixed within a campaign. Write the weights you used into the report, since scores from different
weightings aren't comparable.
