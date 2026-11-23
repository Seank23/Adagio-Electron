# Changelog

All notable changes to Adagio are recorded here. The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and Adagio uses [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added

- **Constant-Q pipeline**: CQT → Peak Extractor → Presence Filter → Note Detector → Key Detector → Chord Predictor. Its spectrum comes from a constant-Q transform, so its bins are equally spaced in pitch and higher notes are measured over shorter windows. The new **CQT** stage sets the window, the frequency range, the bins per octave (12, 24, 36 or 48) and a filter scale. The spectrum view draws its log-spaced bins in place.

### Fixed

- Changing a setting that alters the number of spectrum bins while playing (such as the HPS interpolation factor) could make spectrum smoothing read past the end of older frames. Smoothing now starts over when the size changes.

### Changed

- **Each built-in pipeline can have its own defaults.** A built-in pipeline can now override a stage's default settings, and the Settings tab's changed dot, ↺ button and **Reset all** follow the pipeline's defaults. The built-in pipelines keep today's values for now.
- **`pipelines.json` keeps only the settings you changed** (file version 2), so a future change to a default reaches every setting you left alone. Setting a value back to its default, by ↺ or by typing it, removes it from the file. 0.2.0's file is converted on first launch: values equal to their default are dropped and the rest are kept. A file from a newer Adagio is set aside as `pipelines.json.bad` rather than misread.

### Tests

- Engine unit tests for the log spectrum frame's header and the Constant-Q pipeline's stage names; `smoke.ps1` switches to it while playing, checks it sends log spectrum frames, and changes its bins per octave mid-play.
- UI tests for placing linear and log bins, decoding `startHz`, and routing a log spectrum frame.
- Engine unit tests for pipeline defaults (schema, first frame, resets, refused defaults including the per-track A4, erase-on-default with widened floats, switching) and for `pipelines.json` versions (converting a version 1 file, leaving a version 2 file alone, setting aside a newer one, and never saving defaults).
- `smoke.ps1` and the end-to-end Reset all test now check that an unchanged setting is absent from the file.

## [0.2.0] - 2026-11-05

### Added

- **Settings tab.** The sidebar's Settings tab, disabled in 0.1.0, now works. It lets you choose the analysis pipeline and tune every stage in it while the track plays.
  - The form is built from the schema the engine sends. Each stage is a numbered section, in the order it runs, and you open it by clicking its header. Each setting gets a number field with a slider, a switch, or a choice of options, with its unit (Hz, s, dB, st, bins, ¢) shown beside it.
  - A change applies between frames, and the form shows it only once the engine has accepted it. A value outside the setting's range is refused, with the reason in the status bar. While paused, the current frame is analysed again so you can see the effect.
  - A dot on a stage's header means one of its settings differs from its default. The ↺ button next to a changed setting restores that setting, and hovering over it shows the default. **Reset all** returns every setting in the pipeline to its default, A4 included.
  - Which stage sections are open is remembered.
- **Partial Tracking pipeline**, alongside the existing Harmonic Product Spectrum pipeline (still the default): FFT → Spectrum Filter → Partial Extractor → Harmonic Grouper → Note Selector → Presence Filter → Note Detector → Key Detector → Chord Predictor. It finds individual partials, groups them into harmonic series and picks notes by salience. It comes with three new stages:
  - `PartialExtractor`: picks spectral peaks above a gate (in dB) over a local median, with parabolic interpolation.
  - `HarmonicGrouper`: groups partials into harmonic series within a cents tolerance.
  - `NoteSelector`: chooses up to a set number of notes by harmonic salience inside a frequency range, and stops once candidates fall a set number of dB below the strongest.
- **Presence Filter stage**, in both pipelines. It keeps the detected notes steady from frame to frame: a note has to persist across a presence window before it is reported, an octave hold stops notes jumping between octaves, and an optional spectral mask can be turned on.
- **Key detection algorithm setting.** The Key Detector can match key profiles (Krumhansl–Kessler, the new default) or score against scales (**Scale Match**, 0.1.0's method).
- **Saved analysis settings.** The engine keeps each pipeline's settings and the active pipeline in `%APPDATA%\Adagio\pipelines.json`, and they persist across launches. The file is written 500 ms after the last change and again at shutdown. A file the engine cannot read is set aside as `pipelines.json.bad`, and the built-in defaults load in its place. The A4 reference is the exception: it belongs to the track, so every file still opens at 440 Hz, and switching pipelines keeps it.
- **Protocol:** new `switchPipeline` and `restorePipelineDefaults` commands. `getAnalysisSchema` now also returns `activePipeline` and a `pipelines` list, and schema entries can carry a `unit` and a `persistent` flag.
- **Engine:** a `--pipelines=<path>` command-line option. The app passes its own `userData` file, and an engine started by hand uses `pipelines.json` beside its executable, so a development engine's settings stay separate from the installed app's.

### Changed

- **Harmonic Product Spectrum pipeline order.** The Spectrum Filter now runs before the HPS downsampler, and the new Presence Filter sits between the Peak Extractor and the Note Detector: FFT → Spectrum Filter → HPS Downsampler → Peak Extractor → Presence Filter → Note Detector → Key Detector → Chord Predictor.
- **Default key algorithm** is now Krumhansl–Kessler profile matching, where 0.1.0 used scale matching.
- **Chord score threshold** raised from 0.05 to 0.1, so fewer weak chord candidates are shown.
- **Setting names** no longer include their unit (for example, "Minimum Frequency (Hz)" is now "Minimum Frequency"). The unit appears beside the field instead.
- **Analysis stages** moved to `engine/src/Analysis/Stages/`. Shared spectral helpers (median, dB conversion, normalisation above the noise floor, parabolic interpolation) are now in `AnalysisUtils.h`. The stages are built from a `PipelineStore` registry instead of a hard-coded list.
- **README** rewritten: a screenshot, a Settings tab section, how the pipelines work, and a comparison with MusicAnalyser, Adagio's predecessor.

### Tests

- Engine unit tests for `PipelineStore` (loading, saving, recovery from a bad file, custom pipelines), the stage names in each built-in pipeline, and regression cases for the new note-selection stages.
- `smoke.ps1` covers pipeline switching and restoring defaults over the socket. `smoke.ps1` and `stress.ps1` each run on their own `pipelines.json`, so they start from the defaults and don't touch the one beside the engine.
- UI tests for the Settings tab, and an end-to-end test of switching pipelines during playback and keeping settings across a restart until Reset all.

## [0.1.0] - 2026-10-16

First release.

[Unreleased]: https://github.com/Seank23/Adagio-Electron/compare/0.1.0...HEAD
[0.1.0]: https://github.com/Seank23/Adagio-Electron/releases/tag/0.1.0
