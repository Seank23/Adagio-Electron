---
name: calibrate-pipeline
description: Calibrate an Adagio analysis pipeline's settings (constantQ, hps, partialTracking or a new one) against a sheet-music transcription of a recording. Transcribe the score into ground truth, align it to the audio, drive the real engine over its socket frame by frame, score every frame with a melody- and chord-weighted metric, and sweep settings, biggest levers first, until a target or iteration cap. Then report the best settings with reasons and the pipeline fixes the plateau points to, and optionally make them the pipeline's built-in defaults. Use this whenever the user wants to tune, calibrate, fit, benchmark or evaluate pipeline or stage settings or defaults on a real song, PDF score, MIDI or chord chart; to measure how accurate Adagio's notes, chords or key are on a track; to compare pipelines on real music; or to repeat the How Deep Is Your Love calibration on another track or algorithm. Use it even when the user doesn't say "calibrate".
---

# Calibrating an analysis pipeline against a transcription

The first calibration (constantQ on 60 s of *How Deep Is Your Love*, 2026-11-23) took the pipeline from 0.531 to 0.853
on this skill's metric in 100 engine runs, and became constantQ's built-in defaults. This skill is that session's method
and harness, generalised. `references/case-hdiyl-constantq.md` has what it found; read it before planning a new run.

`$SKILL` below is this skill's directory, and `$W` is the workspace for one track.

## What you need

- **The recording** (WAV) and **the ground truth**: usually a PDF score with a chord chart (piano/vocal/guitar). A MIDI
  file or a chord chart works too: write `truth.json` from it directly.
- **The goal**: which pipeline, the target score (the first brief: 90%) and the iteration cap (100). If the user has
  not said, use those and say so.
- **A Release engine built from the current source**: `cmake --build engine/build --config Release`. Not
  `app/engine-bin`, which is only refreshed by `npm run build` and is often stale.
- **Port 9001 free.** Every run starts its own engine on it, so close Adagio first, and never run two batches at once.
  The scripts refuse to start if the port is taken.
- **Python with `$SKILL/scripts/requirements.txt`.** The system Python lacks websocket-client. Make a venv in the
  scratchpad: `python -m venv <scratch>/calib-venv && <scratch>/calib-venv/Scripts/python -m pip install -q -r
  $SKILL/scripts/requirements.txt`. Use that interpreter for every script below (written `$PY` below; not the Windows `py` launcher, which runs the system Python).

## The workspace

Make one per track in the scratchpad (runs are 2-8 MB of frames each; don't put them in the repo):

```
$W/calibration.json   {"wav": "D:/.../Track.wav", "pipeline": "constantQ",
                       "engine": {"sampleRate": 8000, "frameLength": 8192, "hopSize": 64}}
$W/truth.json         the transcription (step 2)
$W/grid.json          the alignment (step 3)
$W/runs/              log.jsonl, NNN.json frames, the engine's scratch pipelines.json, engine.log
```

Every script takes `--work $W`; file arguments are resolved against the workspace. A relative `wav` is tried against
the workspace, then the repo root. The engine runs with `--pipelines=$W/runs/pipelines.json`, so the user's own
`%APPDATA%\Adagio\pipelines.json` is never touched. To rerun the first track, copy
`$SKILL/examples/how-deep-is-your-love/*` into `$W` (it needs `HowDeepIsYourLove_Analysis.wav` in the repo root).

The scratchpad can be cleared, and this skill is committed in the repo, so what must survive goes back into the skill:

- `examples/<track>/`: truth.json, grid.json, best.json, and a calibration.json whose `wav` is repo-relative.
- `references/case-<track>-<pipeline>.md`: the setup, progression, final settings with reasons, and why it stopped,
  like the HDIYL case.
- Anything learned that applies to every track goes into this file or `references/`.

Never commit the recording or the score PDF (copyrighted, and large). The note-by-note `truth.json` transcribes the
song too, so mention that before the user pushes it anywhere public. Commit only when the user asks.

## Steps

### 1. Set up and read the pipeline
`$PY $SKILL/scripts/schema.py --work $W` lists every stage's settings as the engine reports them: value, range or
options, unit, and which are per-track. Plan from this, not from the stage headers: a built-in pipeline can override its
stages' defaults (`BuiltInStage<T>({...})` in `PipelineStore::BuiltInPipelines()`). Note any setting whose range looks
too narrow; on the first track two of the best values sat at the schema's maximum.

### 2. Transcribe the score → truth.json
Follow `references/transcription.md`. In short: `render_score.py` crops every bar to an image; read them one at a time;
record chords, melody and bass per bar; work out the **form** (which score bar each recorded bar plays; repeats are
common) and any bars the record plays in another octave. Count the bars yourself: the user's count and the PDF's can
differ for good reasons (26 vs 22 on the first track).

### 3. Align it → grid.json
`$PY $SKILL/scripts/grid.py --work $W --bpm <sheet tempo>`. Check that the winning downbeat beats the runner-up clearly.
Then `$PY $SKILL/scripts/inspect_audio.py 1-2,9 --work $W` for a few bars from each section: the truth's melody and bass
should be among what a reference CQT hears. Fix the transcription now. Every run scored against a wrong truth is wasted.

### 4. Tuning
`$PY $SKILL/scripts/tuning.py --work $W` estimates A4. Records are often several cents off 440, and NoteDetector then
rejects in-tune notes. A4 is per track (`"persistent": false`): every config carries `"a4"`, and it can never be a
pipeline default.

### 5. Baseline
Run the pipeline's defaults twice, with A4 440 and with the estimate:

```
echo '[{"name": "defaults"}, {"name": "defaults + A4 441", "a4": 441}]' > $W/b01.json
$PY $SKILL/scripts/iterate.py b01.json --work $W        (run in the background: ~1.75 x track length per config)
```

`$PY $SKILL/scripts/metric.py runs/001.json --work $W` gives the per-bar breakdown, and `diagnose.py` explains it (step 7).

### 6. Sweep, biggest levers first
Coordinate descent from the current best: vary one setting (or a pair that interacts) at a time, keep what improves,
and revisit earlier settings when later ones change the picture. `sweep.py` writes the batches:

```
$PY $SKILL/scripts/sweep.py best --vary PeakExtractor.SCORE_THRESHOLD=0,0.05,0.1 --vary PeakExtractor.MIN_PROMINENCE=0.3,0.75 --grid -o b02.json --work $W
$PY $SKILL/scripts/iterate.py b02.json --work $W
$PY $SKILL/scripts/summary.py --work $W                  (running best starred; --top 10)
```

The base can be `best`, an iteration number, or a config file; runs are named `B<n> + KEY value`, so the log records
what changed from what. Batches of 4-10 configs, one at a time, in the background; read the result before choosing the
next batch, because the best next step depends on which components moved.

The order that worked, and why:
1. **Global confounders**: engine frame (the CQT needs ≥ ~0.9 s or it loses everything below ~100 Hz) and A4. These
   distort every other comparison.
2. **What gets through peak picking.** On constantQ, PeakExtractor `SCORE_THRESHOLD` × `MIN_PROMINENCE`, swept
   jointly, moved the score 0.53 → 0.80. Everything downstream works on what this stage passes. For `hps` it is the same
   stage after the HPS downsampler; for `partialTracking`, PartialExtractor's gate and NoteSelector's note count and dB
   range.
3. **Label stages** (KeyDetector algorithm and window, ChordPredictor threshold and window). They are cheap to settle
   and independent of the notes; only the chord and key columns move.
4. **Note acceptance**: NoteDetector `ERROR_THRESHOLD` (vibrato), PeakExtractor `MAX_FREQ` (just above the song's highest
   melody note), `MIN_SEMITONE_DISTANCE`, `MIN_SNR`, `MAX_PEAKS`.
5. **Smoothing over time**: PresenceFilter window and octave hold.
6. **Back to the chord threshold.** It is a share of the pitch-class histogram, so its best value falls as more notes are
   reported (0.1 → 0.06 on the first track).
7. **The spectrum stage last** (CQT window, bins per octave, filter scale). On the first track nothing beat its
   defaults.

### 7. When it plateaus, diagnose before sweeping more
A plateau is the best of the last ~15 runs within ~0.002 of the overall best (`summary.py` prints both). Then:
- `$PY $SKILL/scripts/diagnose.py runs/NNN.json --work $W`: melody and bass hit/miss; the false notes and what explains
  them (a 3rd/5th harmonic of a real note, or leakage a semitone beside one); each chart chord's predicted names; keys.
  On the first track's best run: 3rd harmonics 35%, semitone leakage 27%, 5th harmonics 7%. No threshold removes those,
  so they point to pipeline changes.
- Per-bar scores (`metric.py`) plus `inspect_audio.py` on the worst bars: a bar bad under every config is usually a
  truth or grid error. Fix it and run `rescore.py`, which rescores all saved runs.
- When precision is stuck while melody and harmony trade against it, the limit is in the pipeline, not its settings.
  Use the remaining iterations to confirm robustness (A4 ±1 Hz, engine frame and hop, neighbouring values) rather than
  micro-steps, and write the pipeline fixes into the report.

### 8. Stop and verify
Stop at the target, at the cap, or on a plateau once the robustness checks are done. Then run
`$PY $SKILL/scripts/realtime.py best --work $W`, which plays the track at volume 0 and scores the live 32 ms events the
UI gets. It should land within ~0.01 of the stepped score (0.847 against 0.853 on the first track).

### 9. Report
Use this structure, in prose and tables:
1. **Headline**: best score, baseline, iterations, whether the target was met, and the real-time score.
2. **Ground truth and alignment**: bar mapping, tuning, any arrangement differences (octave, repeats).
3. **The metric**: the weights table from `references/metric.md`, plus what a perfect and an empty run score.
4. **Recommended settings**: Setting | Default → Best | Why (what it fixes, what neighbouring values did). Then the
   settings tried and left alone, and the engine parameters.
5. **Before → after**: melody exact, bass exact, chord root, key, notes per frame.
6. **Why it stopped, and fixes**: what `diagnose.py` showed and the pipeline changes that would address it, ordered by
   expected gain.
7. **Caveats**: one track (which settings are most song-specific), note density against the score, event size or CPU
   costs.
Give the full paths of the log, the frames and the final config.

### 10. Making the result a pipeline's defaults (only when asked)
- Declare them in `PipelineStore::BuiltInPipelines()` with `BuiltInStage<T>({ .KEY = value })`. Designated initialisers
  must follow the `TSettings` struct's declaration order. A value equal to the stage default is dropped (`IsSameValue`),
  and a `"persistent": false` key (A4) is refused, which would fail ctest's `BuiltInStageNames` check (`warnings.empty()`).
  Add a one-line comment saying what the values were calibrated against.
- Rebuild Release. Run `ctest --test-dir engine/build -C Release` and `pwsh engine/tests/smoke.ps1`.
- Verify: rerun `iterate.py` with a config holding only `"a4"`. A fresh pipeline must reproduce the best run's score
  exactly.
- Update CLAUDE.md's pipeline paragraph and CHANGELOG `[Unreleased]`. Say that `app/engine-bin` is stale until
  `npm run build`. Don't commit unless asked.

## Pitfalls that cost time on the first run

- **"Ping timeout" looks like a corrupt message.** The engine pings every second and drops a client that hasn't read for
  about a second (close code 1011). Scoring between runs does exactly that. `iterate.py` reconnects before every run, and
  `adagio.py` turns a close frame into a `ConnectionError`; it is not an engine bug.
- **stepFrame batch size**: 64 per round trip. 128 dropped connections with dense output; 16 was mostly waiting.
  `iterate.py` retries a dropped run three times on a fresh engine.
- **`restorePipelineDefaults` resets A4 too**, so `apply()` sets A4 last, and every config that wants it must carry it.
- **Label-stage sweeps don't change the note columns.** If they do, something else changed.
- **The total can be gamed by reporting more notes.** Watch n/f and F1 (`references/metric.md`). Say in the report what
  the best run reports against the score's note count, and offer a cleaner alternative if one is close.
- **Song-specific settings**: `MAX_FREQ` follows the melody's range and `ERROR_THRESHOLD` the singer's vibrato. Before
  making one track's values defaults, say which ones another track would most likely move, and offer to check a
  second track.
- **Longer key windows enlarge every analysis event** (the key histogram rides in it): 50 s made events ~51 KB,
  ~1.6 MB/s. Mention it when recommending one.
- **Windows console**: the scripts force UTF-8 stdout for flats and the cents sign; a script of your own needs
  `PYTHONIOENCODING=utf-8`.
- **Runs are slow** (~1.75 × the track's length at hop 64). Budget the cap accordingly, run batches in the background,
  and do the transcription and diagnosis while they run.

## Scripts

| Script | Does |
|---|---|
| `render_score.py PDF OUT` | page images and one upscaled image per bar |
| `grid.py` | beat grid aligned to the truth's bass line → grid.json |
| `inspect_audio.py BARS` | truth beside a reference CQT, per eighth |
| `tuning.py` | A4 estimate |
| `schema.py` | the pipeline's stages and settings from the engine |
| `sweep.py BASE --vary ...` | batch files varying a base config |
| `iterate.py BATCH` | run a batch through the engine, score, log |
| `summary.py` | progress, running best, `--top`, `--best-out` |
| `metric.py FRAMES` | score one run, per-bar breakdown |
| `diagnose.py FRAMES` | where the score is lost; what the false notes are |
| `rescore.py` | rescore every saved run after fixing truth/grid/metric |
| `realtime.py CONFIG` | score real-time playback |
| `adagio.py`, `truth.py`, `workspace.py` | engine driver, truth loader, workspace config |
