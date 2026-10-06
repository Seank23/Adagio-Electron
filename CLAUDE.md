# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

Adagio is a desktop audio analysis/playback app: an Electron + React front end (`app/`) driving a native C++20 DSP engine (`engine/`) that runs as a **separate process**. The engine decodes audio, plays it through miniaudio, time-stretches with RubberBand, and runs a real-time FFT → note → key → chord analysis pipeline, streaming results to the UI.

## Commands

All npm commands run from `app/` (there is no root `package.json`).

```bash
# UI + Electron dev (Vite on :5173, then Electron)
cd app && npm run dev

# UI only
cd app/ui && npm run dev
cd app/ui && npm run lint      # eslint flat config
cd app/ui && npm test          # Vitest + Testing Library in jsdom: invariants, slices, router, components

# End to end: the real Electron app and engine, driven by Playwright (the overhaul guide's §7).
# Starts Vite itself, and needs a Release engine build (or ADAGIO_ENGINE_PATH) and port 9001 free.
cd app && npm run test:e2e

# Engine tests: unit suite, then a real engine driven over its socket
ctest --test-dir engine/build -C Release
pwsh engine/tests/smoke.ps1

# Load/play/clear on a long track, for the "engine went deaf" class of bug
pwsh engine/tests/stress.ps1

# Either can be run against an engine started with --trace, which logs every frame
# received, command queued and command handled.

# Package the desktop app (builds UI into app/dist/ui, then electron-builder into app/release)
cd app && npm run build
```

Engine (CMake + MSVC; Windows-first — `/arch:AVX2`):

```bash
cmake -S engine -B engine/build
cmake --build engine/build --config Release
```

The wire protocol lives in `protocol/protocol.json`. Editing it re-runs CMake's configure step, which regenerates `engine/build/generated/Protocol.generated.h`; the UI imports the same file through the `@protocol` alias, which `vite.config.js` also has to allow in `server.fs`.

Every C++ dependency comes from `FetchContent` at a pinned tag: kfr, IXWebSocket, nlohmann/json, doctest and RubberBand. RubberBand has no CMake build, so `engine/CMakeLists.txt` compiles its `single/RubberBandSingle.cpp` as a static library, with the built-in FFT and resampler its own Visual Studio project used. That file includes the rest of the library by relative path, so it has to be compiled where it is. `README.md` lists the prerequisites. A fresh checkout under a long path can hit Windows' 260-character limit inside nlohmann/json's test tree; build from a short one.

**In dev mode the Electron main process spawns the engine only if `ADAGIO_ENGINE_PATH` points at a binary.** Without it, start `AdagioEngine.exe` yourself before `npm run dev`, or the UI will have no backend on port 9001 — the footer will read "Connecting…" and then "Disconnected". A packaged build always spawns the engine from `process.resourcesPath/engine/` and waits for its `Adagio engine ready` line before opening the window.

An engine main did not start has no token and requires none, so a hand-started one still accepts the renderer. One main spawned is given `--token=<hex>` and refuses anything that cannot present it, plus the saved `--sample-rate=`, `--frame-length=`, `--hop-size=` and `--frame-smoothing=` (each checked by `UpdateEngineParams`; a bad one is a stderr warning and keeps its default). A hand-started engine gets none of those, so it runs with the defaults whatever Preferences saved.

## Architecture

### Three processes, one channel

```
React renderer ═══ WebSocket :9001 (authenticated) ═══> C++ engine
      │   commands  {id, cmd, args}  ───────────────────────>│
      │   replies   {type:"reply", id, ok, value|error}  <───│
      │   events    {type, value}                        <───│
      └── window.api (IPC) ──> Electron main: file dialog, engine token, spawn status, preferences
```

- **Commands and everything the UI observes share one socket.** The renderer sends `{id, cmd, args}`; `main.cpp` parses it on the connection's thread, pushes a `Command` onto the `CommandQueue` singleton, and the command thread answers with a `reply` addressed to the client that asked. Engine code anywhere pushes JSON onto the `MessageQueue` singleton; `WSServer::ProcessQueue` drains it every 5 ms, broadcasting or sending to one client.
- **The protocol is described once, in `protocol/protocol.json`.** CMake turns it into `Protocol.generated.h` (command enum, wire names, argument kinds, event names, limits) at configure time; `app/ui/src/utils/protocol.jsx` imports the same file through the `@protocol` alias. Neither side spells a command or event name by hand.
- **Adding an engine→UI event**: add it to `events` in `protocol.json`, push it with `Protocol::Event::<Name>`, and handle it in `app/ui/src/router/EngineEventRouter.jsx`.
- **High-rate data travels as binary frames**, not JSON: a 32-byte little-endian header (kind, version, element type, seek generation, timestamp, resolution, max magnitude, count), then the data, laid out in `protocol.json`'s `binaryFrames`. `BinaryFrame.h` encodes them, with `static_assert`s pinning the header to those offsets, and `MessageQueue::PushBinary` queues them in order with the text events. The spectrum goes out every hop as Uint16 of `magnitude / max`; the waveform goes out as one Float32 frame per resolution (256 to 8192 samples a peak), before `fileLoaded`. The engine keeps those frames and resends them to any client that asks with `getWaveform`, which the router does on connect when a track is open, so a reload still has its timeline. The renderer decodes them in `engine-client/BinaryFrame.jsx`. `status` reports `bytesSent`, which is what the smoke test's throughput case reads.
- **Adding a UI→engine command**: add it to `commands` in `protocol.json` (which gives you the `CommandType` entry and the parser's argument check), then a row in the `TransportState` table (`engine/src/Core/TransportState.cpp`), an `Application::HandleCommand` case, and a method in `app/ui/src/engine-client/EngineCommands.jsx`. Every command is checked against the table first, so a command with no entry is rejected rather than run.
- **Authentication.** Main generates one token per launch, passes it to the engine on the command line and to the renderer through preload; the renderer puts it in the handshake query. The engine also refuses any handshake whose `Origin` is not in `protocol.json`'s `allowedOrigins`, which is what keeps a page in a browser off the channel whether or not a token is set.

### Engine threading

`Application::Run` is a 2 ms polling loop that drains the command queue on the main thread. It also sends `position` at 30 Hz and, when the callback reports the track has drained, sends `endOfPlay` once and runs Stop itself: the engine ends a track, not the UI. Around it:

- **Feeder thread** (`PcmFeeder::LaunchFeeder`) interleaves the decoded planar PCM a chunk at a time into every registered `RingBuffer` by name. `Application::LoadAudio` registers `"Playback"` (5 s).
- **Audio callback thread** (miniaudio → `PlaybackService::OnAudioCallback`) pulls through `TimeProcessor` (RubberBand), and applies volume. It publishes position and end of track through atomics only: no strings, no locks.
- **Analysis thread** (`AnalysisService::StartAnalysis`) runs while playing and analyses a frame each time the playhead has moved `HopSize × speed` analysis-stream samples, so frames arrive at a fixed rate in wall time (`sampleRate / hopSize`: 62.5 Hz at the default `{8000 Hz, 4096, 128}`, from 15.6 to 500 Hz across the Preferences values) and slower playback samples the source more finely.
- **WebSocket threads**: one per connection (where inbound frames are parsed) plus the queue thread that drains `MessageQueue`. A stdin thread queues `Shutdown` on EOF.

Loops that wait between polls sleep on a `HighResolutionTimer`, not `std::this_thread::sleep_for`: on Windows the latter rounds up to the 15.6 ms timer tick, which capped analysis at about 64 frames a second and stretched the old 2 ms loop eightfold. A timer belongs to the thread that waits on it.

`CommandQueue`/`MessageQueue` are mutex-guarded singletons and are the only sanctioned cross-thread channel — prefer pushing a message over reaching across services. Seeking follows the same rule: the command thread bumps an atomic generation on `PcmFeeder`, the feeder marks each `RingBuffer` with that generation, repositions itself and republishes it, and the audio callback then drops only what was written before the mark. Nothing sleeps waiting for another thread. `FeederState` belongs to the transport: the feeder never changes it itself, not even at end of file. A track ends when `TimeProcessor` reports it has drained, meaning the source is exhausted and RubberBand has been flushed with a final block, not when a frame counter reaches the file length.

`stepFrame` (Ready or Paused only) moves the playhead one analysis hop and analyses that frame with no sound. It weights its frame by one hop as playback would. Neither a step nor a seek clears the analysis trackers: the key decays by source time and the chord window drops frames by their distance from the playhead, so both settle after a jump. Only `resetAnalysis` clears them; while the analysis thread runs it only sets a flag, because that thread owns `PersistentData`, and whichever thread resets pushes the `analysisReset` event, so it lands in order with the `analysis` events. The UI clears its analysis view on that event, never on the command's reply.

`Application` owns the only copy of playback state, as a `TransportState` (`Empty → Loading → Ready ⇄ Playing ⇄ Paused`), and broadcasts a `transport` event (state, position, speed, volume, `track`: path, sample rate, channels, duration and the analysis rate, or `null` with no file, and `analysis`: the `{sampleRate, frameLength, hopSize, frameSmoothing}` in force (`AnalysisService::ParamsJson`, which has to carry every value `EngineParamsJson` does), with or without a file) after every command that changes it, `setEngineParams` included. The UI renders from that and holds no playback state of its own. The `status` command answers the same payload in its reply without broadcasting, so it is both the liveness probe and the way a client that has just connected learns where the engine is. `engine/tests/smoke.ps1` drives a real engine over the socket through the sequences that used to crash it, plus the protocol and authentication rules.

### Analysis pipeline

`AnalysisService` keeps its **own** preprocessed stream, independent of playback: the static `Preprocess` mixes to mono, Butterworth-lowpasses at the new Nyquist when it downsamples (never when it upsamples), and resamples to `EngineParams.Analysis.SampleRate`, then the service loads it whole into a `RingBuffer`.

`EngineParams` (`Core/EngineParams.h`) holds every engine-wide setting a client or the command line may change: today its `Analysis` member (an `AnalysisParams`) and `Debug`. The analysis values default to `{8000 Hz, 4096, 128}` from `protocol.json`'s `limits`, which also list the values allowed (sample rates 4/8/16 kHz, frames 2048–8192, hops 32–256). `FrameSmoothing` (default 4) is a range instead, 1–10 from `frameSmoothingMin`/`Max`: the analysis thread averages the outgoing spectrum over that many frames, 1 for none; the notes and everything downstream see the unaveraged frame. `Application::m_EngineParams` is the one copy outside the service, and the one the `transport` event's `analysis` reports, file or no file: the command line sets it, the next load uses it, and `setEngineParams` (legal in every state but Loading) changes it through `UpdateEngineParams`, which changes nothing unless every value given is on its list (or in its range). With a file open, `AnalysisService::SetParams` stops the analysis thread, writes the values, preprocesses the whole track again for a new sample rate (on the command thread, as a load does), and starts a new thread: the thread reads `m_Params` and the buffer every pass and its spectrum average holds the old bin count, so never write them while it runs. `PersistentData` is kept. `EstimatePlayhead` extrapolates the current playhead as `feeder.GetPlaybackTime() + (now - lastPlaybackFrameTimestamp) × speed`, and `ProcessFrameAt` reads a frame centred on it — so analysis is positioned by timestamp, not by consuming a stream. Each frame carries `DeltaTime`, the source seconds it covers (one hop's worth for an on-demand frame while paused).

Stages run in order in `AnalysisPipeline`, each mutating a shared `AnalysisContext`:

`FFTProcessor → HPSDownsamplerProcessor → SpectrumFilterProcessor → PeakExtractor → NoteDetector → KeyDetector → ChordPredictor`

- A stage is a header-only subclass of `ConfigurableStage<TSettings>` in `engine/src/Analysis/`. `TSettings` is a plain struct of the stage's settings with their defaults, made JSON-convertible with `NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT`; the stage reads them in `Execute` as `m_Settings.KEY`. It overrides `Execute`, `GetType` and `BuildSettingsSchema`, and declares `static constexpr std::string_view Name` with a `GetStageName()` that returns it. Register it in `AnalysisService::CreatePipeline`, which `BuildPipeline` runs once in the service's constructor: the pipeline and its settings outlive the loaded file, so `getAnalysisSchema` answers with nothing open.
- **`Name` is the settings key**, and a client stores settings under it, so never rename one. It used to come from `typeid`, whose names are mangled on every compiler but MSVC. `AnalysisSettingsTests.cpp` pins the production names in order.
- `BuildSettingsSchema()` returns a JSON *schema* (`name`/`type`, plus `options` or `min`/`max`); `Initialise` fills each entry's `default` from `TSettings{}` once, when the stage is added. `SetSetting` stores the change and bumps a version, and `ProcessFrame` calls `ApplySettings` on every stage only when that version has moved, between frames.
- `AnalysisContext::PersistentData` (the key's decayed accumulators, the chord window's frames, previous chord) survives across frames, and across seeks; only `resetAnalysis` clears it. Everything else is per-frame.
- Anything accumulated across frames is weighted by `Frame.DeltaTime`, never counted per frame, so it doesn't change with the hop or the playback speed. The key decays as `H ← H·e^(−dt/τ) + score·dt` with `τ = ROLLING_WINDOW / 3`; the chord window is a hard `ROLLING_WINDOW` of source time, and a root's presence is in seconds.
- `NoteDetector` names notes against its `A440_TUNING` setting (415–466 Hz, default 440), the A4 reference the UI's tuning control edits. Each note in the `analysis` event carries the engine's `midi`; the UI never re-derives a detected note from Hz.
- `KeyDetector` sends the key as `detectedKey`, a `KeySignature` object (`name`, `tonic`, `tonicClass`, `mode`, `scaleClasses`). `name` is empty until a key is detected, and the other fields aren't meaningful until then.
- Results are serialised by `AnalysisPipeline::GetResultJson` into one `analysis` event, sent every 32 ms of wall time while playing (31.25 Hz at any sample rate and hop; don't go back to counting frames) and always for an on-demand frame. The spectrum, its maximum and `binHz` are not in it; they travel in the spectrum frame every hop. Adding a field to the JSON means adding it to `analysisSlice.jsx` too.
- `getAnalysisSchema` returns every stage's schema in pipeline order with the value in force; `setAnalysisSetting` checks a value against that same schema, including any `min`/`max` it declares, and applies it between frames, so a change never lands mid-frame. While paused nothing runs, so the UI follows a setting change with `analyseFrame`.

### UI

Vite + React 19 + Ant Design 6 (sliders, tooltips, the theme algorithm), emotion `styled` for everything else, Lucide icons, Inter and JetBrains Mono bundled with `@fontsource`. Redux Toolkit for all shared state. Slices: `app` (status messages, engine connection, the file a load is waiting on, `preferencesOpen`), `playback` (the engine's transport — state, speed, volume, `track` — plus current time), `analysis` (notes, histograms, key, chords), `pipeline` (the engine's analysis schema with the values in force, fetched on connect and patched only on an `ok` reply; `selectTuningHz` reads the A4 reference from it; and `params`, the engine's `{sampleRate, frameLength, hopSize, frameSmoothing}` from every transport event and status reply), `settings` (UI-only: theme mode, log scale, repeat, follow, sidebar tab). The spectrum and the waveform peaks are typed arrays from binary frames and live outside Redux, in `engine-client/FrameStore.jsx`.

**Theme.** `theme/tokens.jsx` holds both palettes and is the only file with colour literals. `AppThemeProvider` resolves the mode (it follows `prefers-color-scheme` when `settings.themeMode` is `system`) and publishes the palette three ways: antd `ConfigProvider` tokens, CSS custom properties on `<html>` (`--bg-panel`, `--accent-primary-fg`, …) that the styled components read, and `PaletteContext` (`usePalette()`) for the canvases, which read it once per theme change, never through `getComputedStyle` in a draw.

**Layout.** `App.jsx` is a grid that fills the window and never scrolls: top bar 44, then transport (64) and timeline sharing a quarter of the remaining height (the timeline never below 120), the body three quarters, status bar 26. The body is the main column and a 320 px sidebar; the main column splits 3 : 1 between the spectrum and the note activity panel. With no file open, an `EmptyState` drop card replaces the timeline and body. The engine starts at 20% volume (`PlaybackService::m_Volume`); the UI never sets an initial volume, so a reload keeps whatever the engine has.

Nothing is set optimistically. `playbackSlice` exports `selectIsPlaying`, `selectIsPaused` and `selectIsFileOpen`, all derived from the one transport state, so the UI cannot drift from the engine the way it did when it guessed. Components send commands through `useEngineCommands()`, which answers `{ok, value, error}` once the engine has run the command.

**Preferences** are app-wide only (theme, analysis sample rate, frame size, hop size, spectrum smoothing, About), in `components/preferences/PreferencesModal`, an antd `Modal` opened by the top bar's cog or Ctrl+, from any screen; while it's open the global shortcuts do nothing. Per-stage analysis settings belong to the sidebar's future Settings tab, not here. Each preference asks its owner and shows the owner's answer (`hooks/usePreferences.jsx`): the theme goes to main, which saves `preferences.json` in `userData` (written aside and renamed), sets `nativeTheme.themeSource` before the window exists so the frame and `prefers-color-scheme` agree from the first paint, and answers the saved preferences, which `settings.themeMode` then mirrors. The analysis values go to the engine through `setEngineParams`; the chips and the smoothing stepper move on the transport event, and only the engine's `ok` answer is passed to main to save, under `preferences.json`'s `engine` key. Nothing preference-related touches `localStorage`. About's version and date come from Vite's `define` (`__APP_VERSION__` from `app/package.json`, `__BUILD_DATE__` the local build date) through `utils/appInfo.jsx`.

`EngineEventRouter` is a render-less component mounted once in `App.jsx`; it is the **only** place engine signals become dispatches or `FrameStore` writes. That covers WebSocket events, binary frames, the socket's own connection state (`useEngineConnection`) and the engine process status main pushes over `window.api.onEngineStatus`. Components read via `useSelector` (or `useStoreListener`, for canvases) and never touch the socket. `WebSocketProvider` wraps the Redux `Provider` in `main.jsx`.

The engine's health reaches the UI as two independent facts, both shown in the footer: `app.connectionState` is whether the socket is up (`connecting`/`connected`/`disconnected`, reconnecting with backoff), and `app.engineStatus` is what main knows about the process (`starting`/`ready`/`external`/`failed`/`exited`, with an error string). A spawn failure is worth showing while the socket is still retrying, which is why both exist.

`SpectrumCanvas` is imperative 2D canvas, not React elements — at one spectrum frame per hop, re-rendering per frame is not viable. It runs one `requestAnimationFrame` loop per mount that reads `getLatestSpectrum()` and redraws only when the frame, the notes or the size have changed, so neither restarts the loop; keep new data out of that effect's dependencies. `FrameStore` drops a spectrum frame from an older seek generation than the one shown. Every canvas sizes its backing store with `fitToElement` (`utils/canvas.jsx`) at the device pixel ratio and draws in CSS pixels.

The spectrum's frequency axis comes from `makeAxis` and `spectrumMaxHz` in `utils/frequencyAxis.jsx`. `ChordKeyboard`, the note activity panel's chord-window keyboard, uses the same functions with the same maximum and width but always the log scale, so its keys stay equally spaced and sit under the spectrum's peaks while the spectrum is on Log. Both take the axis's top from `selectAnalysisRate` (the last analysis event's rate, or the track's before the first event), so the grid and the keyboard draw as soon as a track is open. The spectrum header shows the frequency range and, beside it, the smoothing in force (`formatSmoothingSummary`: frames and the time they span, from the engine's `params`). Its keys follow `selectTuningHz`, and it dots the notes the engine detected in the frame by their `midi`.

Per-event data reaches the screen without React re-renders. A component follows a store value with `useStoreListener` (`hooks/useStoreListener.jsx`), which calls back on change without re-rendering, or `useStoreFrameListener`, the same coalesced to one paint per animation frame. The keyboard paints its canvas that way, and the sidebar writes its pitch-class and chord bars' widths and percentages through refs. A component that selects per-event arrays with `useSelector` re-renders 31 times a second, so select the primitive it displays instead (`predictedChords[0]?.name`, not `predictedChords`). `analysisSlice` keeps the same `detectedKey` object while its name is unchanged for the same reason. The timeline (`TimelinePanel`, with `TimelineOverview`, `TimelineRuler` and wavesurfer in `AudioTimeline`) keeps its zoom view in a ref and redraws the ruler and overview from it, not through state. In dev, `utils/devPerf.jsx` logs per-frame script time and each timed section every 5 s; `localStorage.setItem('adagio:perf', 'off')` silences it.

## Conventions and traps

- **UI tests** live in `app/ui/test/`. `helpers.jsx` has a `FakeEngine` with `WebSocketEngine`'s listener API and scripted replies, and a fresh store per test. `invariants.test.jsx` reads the source for the architectural rules (no colour literals outside `tokens.jsx`, only the router subscribes to the engine or writes `FrameStore`, and so on), so breaking one fails `npm test`, not just review. jsdom has no canvas, so what a canvas draws is checked end to end.
- **End-to-end tests** live in `app/e2e/`. `harness.js` writes 48 kHz WAV fixtures (a whole number of samples per hop at every allowed rate and hop: 768 at the default; the step test reads the default from `protocol.json`), launches Electron with `ADAGIO_E2E=1` (no DevTools window), its own `ADAGIO_USER_DATA` directory (so `preferences.json` never leaks between tests) and the engine spawned with a token, and answers the open dialog in main. antd's modal moves focus in, and starts listening for Esc, only once its zoom has ended: in e2e wait for focus inside the dialog before pressing Esc, and in jsdom (no CSS animations) render it under `ConfigProvider theme={{ token: { motion: false } }}`. The canvases carry `role="img"` and a label, and the waveform `role="group"`, so the tests find them by name. Pace `.` presses with `stepFrames`: the step gate drops a press under 33 ms after the last. `e2e/README.md` lists what stays manual.
- C++ uses tabs, Allman braces, `m_` members, `Adagio` namespace, PascalCase methods. Most analysis stages are header-only; services are `.h`/`.cpp` pairs.
- JS is 4-space, single quotes, arrow-function components, `.jsx` extension for *every* source file including slices, hooks, and utils.
- The WebSocket client lives in `app/ui/src/engine-client/`: `WebSocketEngine.jsx` (the socket, with reconnect backoff, a `CONNECTION_STATE` and the pending-reply map behind `request()`), `WebSocketContext.jsx` (the context alone), `WebSocketProvider.jsx` (the component, which fetches the token before opening) and `EngineCommands.jsx` (one method per command). The context and the provider are separate files because fast refresh wants a module to export components *or* values, not both.
- Volume and speed travel normalised (0–1, and 0.2–1.0 from `protocol.json`'s `limits`, which the engine clamps to as well). Percentages exist only in the sliders, which convert at the boundary in `PlaybackControls`.
- The engine binary lives in `app/engine-bin/`, which holds nothing else. Keep source out of it: electron-builder copies the whole folder into the packaged resources.
- `electron-builder.yml` copies `app/engine-bin/` to `resources/engine/` and `main.js` spawns `<resourcesPath>/engine/AdagioEngine.exe`, so the built engine binary must be dropped into `app/engine-bin/` before packaging. Its `directories.output` is `release`, not `dist`: electron-builder excludes its own output directory from the packaged files, and `app/dist/ui` is what Vite builds.
- **The WebSocket server's ping interval has to stay non-zero** (`WSServer::PingIntervalSeconds`). It is what bounds the connection thread's poll: at the library default the poll waits forever, and a wake-up missed while a large message is written from another thread parks that connection permanently — the socket stays open, the engine stays healthy, events keep queueing, and nothing the client sends is read again. It reproduces on any track long enough for the waveform event to need several of the library's 32 KB fragments, and only in Release, where the timing is tight enough. `engine/tests/stress.ps1` is the reproducer.
- `WSServer` never sends while holding its client lock: it copies the `shared_ptr`s out and writes outside it, because the inbound path needs that same lock to check a frame came from a client that was accepted.
- The engine's stdin is spawned as a pipe, not `'ignore'`. The engine shuts down on stdin EOF, so main closes it on `will-quit` to free port 9001, and `'ignore'` would look like an immediate EOF.
- `window.api` is down to what only main can do: `selectAudioFile`, `getPathForFile` (a dropped file's path, through `webUtils`, since Electron removed `File.path`; `''` for none), `getEngineToken`, `onEngineStatus`, and `getPreferences`/`setPreferences` (a patch, merged and saved by main). Its handlers answer `{ok, value}`/`{ok, error}` or a plain value and never throw. `engine.request()` follows the same contract, so a caller checks `ok` rather than relying on a rejection; a dropped socket answers every command in flight instead of leaving it hanging.
- The port lives in `protocol.json` and reaches both sides from there — `Protocol::Port` in C++, `ENGINE_WS_URL` in the UI. Nothing hardcodes 9001 any more.
- No page opens a window: main's `setWindowOpenHandler` denies every one and hands `https://github.com/` URLs to `shell.openExternal`. An external link needs `target="_blank"` to reach it; without one it would navigate the app window itself.
