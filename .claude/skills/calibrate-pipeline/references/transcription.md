# Ground truth: from sheet music to truth.json and grid.json

The metric is only as good as the transcription and its alignment. On the first track, half a day of
engine runs would have been wasted if the bar mapping had been wrong, and the intro would have looked like a pipeline
failure if nobody had noticed the record plays it an octave above the sheet. Spend the time here.

## Contents
1. truth.json format
2. Reading the score
3. The form: recorded bars vs score bars
4. Aligning: grid.py
5. Checking it before you trust it

## 1. truth.json format

```json
{
  "title": "Song, first 60 s",
  "source": "the PDF and the WAV",
  "notes": ["anything a later reader needs: which line is the melody, arrangement differences"],
  "beatsPerBar": 4,
  "key": {"Eb Major": 1.0, "C Minor": 0.5},
  "form": [1, 2, 3, 4, 1, 2, 3, 4, 5, 6],
  "looseOctaveBars": [1, 2, 3, 4, 5, 6, 7, 8],
  "chordTones": {"Bb7alt": {"roots": ["Bb"], "tones": ["Bb", "D", "Ab", "B", "Db", "Gb"]}},
  "bars": [
    {"bar": 1, "chords": [[0, "Eb"]],
     "melody": [[0.5, 0.5, "Bb3"], [1, 0.5, "Eb4"], [1.5, 1, "F4"], [2.5, 1.5, "G4"]],
     "bass": [[0, 3.5, "Eb2"], [3.5, 0.5, "Bb2"]]},
    {"bar": 2, "chords": [[0, "Fm7"], [2, "C7"]], "melody": [], "bass": [[0, 2, "F2"], [2, 2, "C2"]]}
  ]
}
```

- **Notes** are `[start beat, length in beats, pitch]`. Beats count from 0. The pitch is a name (`"Bb3"`, `"F#5"`; C4 = 60
  = middle C) or a MIDI number. Triplet eighths are 1/3 beat (write `0.3333`, `0.6667`).
- **`key`** maps the engine's key names (`"<Tonic> Major|Minor"`, tonic spelled with flats: C Db D Eb E F Gb G Ab A Bb B)
  to credit. Give the relative minor (or major) half credit: Scale Match names the relative minor whenever its tonic
  is louder, which says more about the algorithm than about the analysis.
- **`chords`** are `[beat, name]` as the chart writes them. A bar without chords keeps the last one; `"N.C."` is no
  chord. Tones come from the name (`maj7`, `m7`, `7`, `9`, `m6`, `sus4`, `dim7`, `m7b5`, `6/9`, `add9`, slash chords,
  `min`/`-`/`M7`/`Δ`/`ø`/`°` spellings, bracketed extensions). For anything else, add it to `chordTones`; `Truth` raises
  rather than guess. A slash chord's bass note counts as a second root and a tone: `Ab/Bb` = Ab, C, Eb + Bb.
- **`bar`** is only for the reader; order is what counts.
- `scripts/truth.py` loads and checks it: `python -c "import sys; sys.path.insert(0, 'SKILL/scripts'); from truth import
  Truth; t = Truth('truth.json'); print(len(t.bars), t.chords)"`.

## 2. Reading the score

1. `python scripts/render_score.py score.pdf score/` renders the pages and crops every bar (with the chord symbols and
   vocal line above it) to `score/barNNN.png`. It found all 22 bars of the first PDF. Read the bars one at a time
   with the Read tool; a whole page is too small to read noteheads and accidentals reliably.
2. For each bar record:
   - **chords**: the chord symbols over the vocal staff, at the beat they sit over. A chord name cut off at the
     bottom edge of a crop belongs to the next system.
   - **melody**: the vocal line. Where nothing is sung (intro, solos), use the top line of the piano's right hand,
     the instrumental lead. Two simultaneous melody voices (a falsetto over a lead pickup) are both listed and both
     required.
   - **bass**: the piano's bass clef notes. Both notes of an octave or fifth dyad go in, since both are played.
3. Reading traps:
   - Apply the key signature to every note it covers, and an accidental to the rest of its bar.
   - Treble clef: bottom line E4, top line F5. Bass clef: bottom line G2, top line A3. Count ledger lines.
   - Fold a tie into the first note's length. A tie across a barline: shorten the first note to the barline and
     start the rest at beat 0 of the next bar.
   - Ignore grace notes, and lyrics' melisma dashes; ignore guitar chord diagrams.
   - A pickup (anacrusis) is its own entry with notes at their real beats (`[3, 1, "Bb3"]` in a 4/4 bar), listed in
     `form`.
4. **Bar count.** Count the bars in the PDF yourself and compare with what the user said. On the first track the user
   said 26 bars and the PDF had 22: the recording plays the 4-bar intro twice. Neither was wrong. That is what `form`
   is for.

## 3. The form: recorded bars vs score bars

Sheet music is an arrangement, not the record. `form[n - 1]` is the score bar that recorded bar `n` plays:
`[1, 2, 3, 4, 1, 2, 3, 4, 5, ..., 22]` for an intro played twice. Look for repeat signs, D.S./D.C., codas, vamps,
"repeat and fade". If the recording plays a section the sheet doesn't have, transcribe it from the audio as an extra
score bar or end the analysis before it (trim `form`; the metric ignores frames outside the grid).

**`looseOctaveBars`** (recorded bar numbers) are where the record plays the line in a different octave from the sheet;
any octave of the melody gets full credit there. On the first track: the whole intro (recorded bars 1-8). Find them
with `inspect_audio.py`, not by guessing, and say in `notes` why each one is loose.

## 4. Aligning: grid.py

`python scripts/grid.py --bpm <sheet tempo>` beat-tracks the WAV, extends the beats on the median interval, and tries
every candidate downbeat for bar 1, scoring how often the truth's bass note is the loudest bass pitch class. It
writes `grid.json` (one entry per beat: `audioBar`, `scoreBar`, `beat`, `time`, `next`).

Read its printout:
- The winner should beat the runner-up clearly (first track: 0.220 against 0.191 and 0.155). A near tie means the form,
  the bass transcription or the tempo is wrong.
- The tracked tempo should match the sheet's or be close. A half- or double-time lock gives nonsense: pass `--bpm`.
- "the form needs N bars but the recording ends first" means `form` is too long for the excerpt.

It assumes a steady tempo, which holds for most studio pop. For rubato, a tempo change or a fermata, split the track
and align the sections separately, or write `grid.json` by hand from barline times you measure: it is just that list.

## 5. Checking it before you trust it

- `python scripts/inspect_audio.py 1-2,9,17` prints, for every eighth of those recorded bars, what the truth says
  (chord, melody, bass) beside the strongest notes a reference CQT hears. Do the first bar, a bar from each section, and
  any bar with an unusual figure. The melody and bass should appear among what is heard in most eighths, though not
  always as the strongest; that is the point of comparing against Adagio.
- After the baseline run, `python scripts/metric.py runs/001.json` prints a per-bar breakdown. A bar where every
  configuration scores badly is more likely a transcription or alignment error than an engine one: check it with
  `inspect_audio.py` before tuning anything for it.
- If you fix `truth.json` or `grid.json` mid-campaign, run `scripts/rescore.py` so earlier runs remain comparable.
