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
cd app/ui && npm run lint      # eslint flat config; there are no tests in this repo

# Package the desktop app (builds UI into app/dist/ui, then electron-builder)
cd app && npm run build
```

Engine (CMake + MSVC; Windows-first — `/arch:AVX2`, and RubberBand is built via `msbuild`):

```bash
cmake -S engine -B engine/build
cmake --build engine/build --config Release
```

`engine/external/rubberband` is gitignored and `.gitmodules` is empty — RubberBand must be placed there manually before the engine will configure. kfr, IXWebSocket, and nlohmann/json are pulled by `FetchContent`.

**In dev mode the Electron main process does not spawn the engine** (`spawnEngine()` is guarded by `!isDev` in `app/electron/main.js`). Start `AdagioEngine.exe` yourself before `npm run dev`, or the UI will have no backend on ports 5000/9001.

## Architecture

### Three processes, two channels

```
React renderer ──window.api (IPC)──> Electron main ──HTTP POST :5000──> C++ engine
      ^                                                                      |
      └──────────────── WebSocket :9001 (JSON events) ───────────────────────┘
```

- **Commands flow one way over HTTP.** `preload.js` exposes `window.api.{load,play,pause,stop,clear,seek,changeVolume,changeSpeed}`; each is an `ipcMain.handle` in `main.js` that POSTs a bare-string body to `http://127.0.0.1:5000/<verb>`. The engine's `main.cpp` turns each route into a `Command` on the global `CommandQueue` singleton and returns immediately — HTTP responses carry no result.
- **Everything the UI observes flows back over WebSocket.** Engine code anywhere pushes a JSON string onto the `MessageQueue` singleton; `WSServer::ProcessQueue` drains it every 5 ms and broadcasts to all clients.
- Adding a new engine→UI event means: push JSON with a new `type` in C++, add the string to `EVENT_TYPE` in `app/ui/src/utils/utils.jsx`, and handle it in `app/ui/src/router/EngineEventRouter.jsx`. Adding a new UI→engine command means: `svr.Post` route + `CommandType` enum entry + `Application::ProcessCommands` case, then IPC handler + preload method.

### Engine threading

`Application::Run` is a 2 ms polling loop that drains the command queue on the main thread. Around it:

- **Feeder thread** (`AudioDecoder::LaunchFeeder`) reads interleaved PCM into every registered `RingBuffer` by name. `Application::LoadAudio` registers `"Playback"` (5 s).
- **Audio callback thread** (miniaudio → `PlaybackService::OnAudioCallback`) pulls through `TimeProcessor` (RubberBand), applies volume, and emits a `position` event every 4th callback.
- **Analysis thread** (`AnalysisService::StartAnalysis`) runs while playing, sleeping `IntervalMs - executionTime`.
- **HTTP and WebSocket threads** from `main.cpp`.

`CommandQueue`/`MessageQueue` are mutex-guarded singletons and are the only sanctioned cross-thread channel — prefer pushing a message over reaching across services.

### Analysis pipeline

`AnalysisService` keeps its **own** preprocessed stream, independent of playback: `PreprocessStream` mixes to mono, Butterworth-lowpasses, and resamples to `AnalysisParams.SampleRate` (currently `{8000 Hz, 4096}`, set in `Application::LoadAudio`), then loads it whole into a `RingBuffer`. Each tick, `ProcessCurrentFrame` estimates the current playhead as `decoder.GetPlaybackTime() + (now - lastPlaybackFrameTimestamp)` and reads a frame centred on it — so analysis is positioned by timestamp, not by consuming a stream.

Stages run in order in `AnalysisPipeline`, each mutating a shared `AnalysisContext`:

`FFTProcessor → HPSDownsamplerProcessor → SpectrumFilterProcessor → PeakExtractor → NoteDetector → KeyDetector → ChordPredictor`

- A stage is a header-only subclass of `AnalysisStage` in `engine/src/Analysis/`, overriding `Execute`, `GetType`, and `GetSettings`. Register it with `m_Pipeline->AddStage(...)` in `AnalysisService::Init`.
- `GetSettings()` returns a JSON *schema* (`name`/`type`/`default`, plus `options` or `min`/`max`). `AddStage` harvests the defaults into the pipeline's settings map, keyed by `GetName()` — which is derived from `typeid(*this).name()`, so **the class name is the settings key**. Read values inside `Execute` via `GetSetting<T>(settings, "KEY")`; never read `context->Settings` directly.
- Call `AnalysisStage::Execute(context)` first in any override — it primes the settings definition that `GetSetting` depends on.
- `AnalysisContext::PersistentData` (rolling notes, previous chord) survives across frames; everything else is per-frame.
- Results are serialised by `AnalysisPipeline::GetResultJson` into one `analysis` event. Adding a field there means adding it to `analysisSlice.jsx` too.

### UI

Vite + React 19 + Ant Design 6, Redux Toolkit for all shared state. Slices: `app` (file open, canvas width, status messages), `playback` (duration, current time, waveform peaks), `analysis` (spectrum, notes, histograms, key, chords), `settings`.

`EngineEventRouter` is a render-less component mounted once in `App.jsx`; it is the **only** place WebSocket events become dispatches. Components read via `useSelector` and never touch the socket. `WebSocketProvider` wraps the Redux `Provider` in `main.jsx`.

Visualisation (`SpectrumCanvas`, `RollingNotesHeatMap`) is imperative 2D canvas drawn in `requestAnimationFrame`, not React elements — at ~5 ms analysis intervals, re-rendering per frame is not viable.

## Conventions and traps

- C++ uses tabs, Allman braces, `m_` members, `Adagio` namespace, PascalCase methods. Most analysis stages are header-only; services are `.h`/`.cpp` pairs.
- JS is 4-space, single quotes, arrow-function components, `.jsx` extension for *every* source file including slices, hooks, and utils.
- `app/engine/` (WebSocket client) sits **outside** `app/ui/src`, so UI files import it with `../../engine/...`. It is intentionally outside the Vite source root; keep the relative paths intact.
- `app/ui/src/store/PlaybackSlice.jsx` is capitalised on disk but imported as `./playbackSlice` everywhere. This only resolves on case-insensitive filesystems (Windows/macOS). Fix the filename if the build ever moves to Linux.
- `electron-builder.yml` copies `app/engine/` into the packaged resources and `main.js` spawns `<appPath>/engine/AdagioEngine.exe`, so the built engine binary must be dropped into `app/engine/` before packaging.
- Ports 5000 (HTTP) and 9001 (WebSocket) are hardcoded on both sides. `WSServer`'s constructor ignores its `port` argument and hardcodes 9001.
