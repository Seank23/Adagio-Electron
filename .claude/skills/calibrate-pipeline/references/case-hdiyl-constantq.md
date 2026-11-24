# Case study: constantQ on How Deep Is Your Love (2026-11-23)

The first calibration, and the source of constantQ's built-in defaults. Use it as a starting point and a sanity check:
what moved the score, what didn't, and where settings ran out.

## Setup

- `HowDeepIsYourLove_Analysis.wav`: the first 60 s of the record, 44.1 kHz. `HowDeepIsYourLove_Transcription.pdf`:
  Musicnotes piano/vocal/guitar, E♭ major, 4/4, 22 bars. Both untracked in the repo root.
- The record plays the 4-bar intro twice (26 recorded bars), partly an octave above the sheet. It is about +8 cents
  sharp (A4 ≈ 441-442).
- Engine at 8 kHz / 8192 / hop 64 (the user's Preferences). About 7,500 frames and ~105 s a run; 100 runs took ~3 h
  of engine time, in background batches of 4-10.
- Truth, grid and the best config are in `examples/how-deep-is-your-love/`.

## Progression (running best)

| Iter. | Total | Change from the previous best | What moved |
|---|---|---|---|
| 1 | 0.531 | constantQ stage defaults | melody 0.31, n/f 2.2: almost nothing gets through peak picking |
| 2 | 0.563 | A4 442, key window 50 s | key 0.50 → 0.74 |
| 4 | 0.617 | MIN_PROMINENCE 0.3, Scale Match | key 0.83 |
| 6 | 0.660 | SCORE_THRESHOLD 0.15, prominence 0.75 | melody 0.55 |
| 8-13 | 0.749 → 0.805 | SCORE_THRESHOLD 0.1 → 0, prominence 0.3 | melody 0.78, n/f 10.7. **The biggest single lever.** |
| 24 | 0.808 | MAX_FREQ 1000 | precision +0.01 |
| 26 | 0.815 | ERROR_THRESHOLD 35 | melody 0.82 (vocal vibrato) |
| 29 | 0.820 | chord window 0.8 | |
| 31 | 0.826 | ERROR_THRESHOLD 45 | |
| 33 | 0.839 | chord SCORE_THRESHOLD 0.07 | chord 0.69 → 0.76: denser notes had starved the chord histogram |
| 42-53 | 0.841 → 0.848 | chord 0.06 / 0.6 s, OCTAVE_HOLD 0.5, MIN_SEMITONE_DISTANCE 0.75 | small steps |
| 64-68 | 0.851 → 0.852 | ERROR_THRESHOLD 50, MIN_SNR 2 | plateau from ~53 on |
| 87 | 0.853 | A4 441 | |
| 100 | 0.8535 | chord window 0.65 | final; real-time playback 0.847 |

## Final settings and why

| Setting | Stage default → best | Why |
|---|---|---|
| PeakExtractor SCORE_THRESHOLD | 0.25 → 0 | The peak score suits a linear spectrum. On a CQT the vocal's peaks scored under 0.25 against the dense accompaniment. |
| PeakExtractor MIN_PROMINENCE | 1.5 → 0.3 | At 36 bins/octave a note spans ~3 bins, so its peak barely stands above its neighbours. 0.2 adds clutter; 0.35 loses melody. |
| PeakExtractor MAX_FREQ | 4000 → 1000 Hz | The highest sung note is G5 (784 Hz); above it was harmonics. **Song-specific**: ~1200 for a soprano. |
| PeakExtractor MIN_SEMITONE_DISTANCE | 0.5 → 0.75 | Merges the duplicate peaks one note makes in a 36-bin CQT. 1.1 starts deleting real semitone pairs (D/E♭ in E♭maj7). |
| PeakExtractor MIN_SNR | 0.2 → 2 | Fewer notes for the same score; 3 cost key stability. |
| PresenceFilter OCTAVE_HOLD | 0.35 → 0.7 s | Small gain; 0 cost 0.016. |
| NoteDetector ERROR_THRESHOLD | 25 → 50 ¢ | Vocal vibrato; each step up helped. 50 is the schema's maximum. |
| NoteDetector A440_TUNING | 440 → 441 | Per track: set it for every recording (`tuning.py`). Can't be a default. |
| KeyDetector ALGORITHM, ROLLING_WINDOW | Krumhansl, 20 → Scale Match, 50 s | Key 0.50 → 0.89. Krumhansl degrades as more notes are reported. 50 is the maximum. |
| ChordPredictor SCORE_THRESHOLD | 0.1 → 0.06 | It is a share of the histogram, so with ~13 notes a frame real 7ths fell under 0.1. 0.03 too low, 0.15 cost 0.13 of chord. |
| ChordPredictor ROLLING_WINDOW | 0.5 → 0.65 s | Small gain; 0.3 and 1.0 both worse. |

Tried and left at their defaults: every CQTProcessor setting (Hann, Blackman-Harris and Rectangle windows; 24 and 48
bins; filter scale 0.75; MIN_FREQ 50), PeakExtractor MAX_PEAKS (10 hurt, 20-24 no better), USE_INTERP_BINS and MIN_FREQ,
PresenceFilter PRESENCE_WINDOW (0.1-0.4).

Engine parameters: 4096 at 8 kHz scored 0.819, because the CQT silently drops everything below ~100 Hz when the frame
is shorter than ~0.9 s (a bin needs Q / f seconds; Q ≈ 51 at 36 bins/octave). 4096 at 4 kHz (the same 1.02 s) scored
0.843.

Before → after: sung melody exact 15% → 78%; bass exact 51% → 90%; chord root 54% → 81%; key E♭ major 50% → 86%;
notes per frame 2.2 → 12.8 (the score has 5-6).

## Why it stopped at ~0.85

1. **Harmonics.** Precision tops out at ~0.80. On the best run `diagnose.py` puts 35% of false notes down to 3rd
   harmonics of real notes (F over B♭, D over G: F4, D5 and F5 are the commonest false notes) and 7% to 5th harmonics.
   Every setting traded found notes for false ones. constantQ has no harmonic suppression; a stage that subtracts each
   strong note's harmonics (+12, +19, +24, +28 semitones on a log axis), strongest first, is the largest gain available.
2. **PeakExtractor isn't designed for a CQT**: 27% of false notes sit a semitone beside a real one (the E2, G♭2 and B1
   beside the bass). Measure prominence over ±1 semitone rather than ±1 bin, and "drop a peak within 1.5 semitones of a
   much louder one" instead of a hard minimum distance.
3. **Automatic tuning** from NoteDetector's average tuning error would let ERROR_THRESHOLD stay tight.
4. **ChordPredictor**: its threshold is a share, so the right value moves with note density; it can't name slash
   chords (A♭/B♭ scores 0.5-0.63 a bar); D♭9 reads as D♭maj7 60% of the time. Template matching would fix all three.
5. **Key**: a borrowed D♭ at 45-51 s tips Scale Match to A♭ major and then to F or C minor. Hysteresis would help.
6. **Event size**: the 50 s key window makes each analysis event ~51 KB (~1.6 MB/s while playing), twice the earlier
   768 KB/s budget. The key histogram is 83-94% of each event.

## What happened next

The user asked for the settings to become constantQ's defaults. They were declared with `BuiltInStage<T>({...})` in
`PipelineStore::BuiltInPipelines()`; ctest (109/109) and `smoke.ps1` passed, and a fresh constantQ with only A4 441 set
reproduced 0.8535 exactly. CLAUDE.md and CHANGELOG [Unreleased] describe them.

**A second track should start from these defaults** and check above all MAX_FREQ (song range), ERROR_THRESHOLD (vibrato)
and the chord threshold (note density), the three most likely to be specific to this recording.
