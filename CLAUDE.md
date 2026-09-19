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

# Package the desktop app (builds UI into app/dist/ui, then electron-builder into app/release)
cd app && npm run build
```

Engine (CMake + MSVC; Windows-first — `/arch:AVX2`, and RubberBand is built via `msbuild`):

```bash
cmake -S engine -B engine/build
cmake --build engine/build --config Release
```

`engine/external/rubberband` is gitignored and `.gitmodules` is empty — RubberBand must be placed there manually before the engine will configure. kfr, IXWebSocket, and nlohmann/json are pulled by `FetchContent`.

**In dev mode the Electron main process spawns the engine only if `ADAGIO_ENGINE_PATH` points at a binary.** Without it, start `AdagioEngine.exe` yourself before `npm run dev`, or the UI will have no backend on ports 5000/9001 — the footer will read "Connecting…" and then "Disconnected". A packaged build always spawns the engine from `process.resourcesPath/engine/` and waits for its `Adagio engine ready` line before opening the window.

## Architecture

### Three processes, two channels

```
React renderer ──window.api (IPC)──> Electron main ──HTTP POST :5000──> C++ engine
      ^                                                                      |
      └──────────────── WebSocket :9001 (JSON events) ───────────────────────┘
```

- **Commands flow one way over HTTP.** `preload.js` exposes `window.api.{load,play,pause,stop,clear,seek,changeVolume,changeSpeed}`; each is an `ipcMain.handle` in `main.js` that POSTs a bare-string body to `http://127.0.0.1:5000/<verb>`. The engine's `main.cpp` turns each route into a `Command` on the global `CommandQueue` singleton and returns immediately — HTTP responses carry no result.
- **Everything the UI observes flows back over WebSocket.** Engine code anywhere pushes a JSON string onto the `MessageQueue` singleton; `WSServer::ProcessQueue` drains it every 5 ms and broadcasts to all clients.
- Adding a new engine→UI event means: push JSON with a new `type` in C++, add the string to `EVENT_TYPE` in `app/ui/src/utils/utils.jsx`, and handle it in `app/ui/src/router/EngineEventRouter.jsx`. Adding a new UI→engine command means: `svr.Post` route + `CommandType` enum entry + a row in the `TransportState` table (`engine/src/Core/TransportState.cpp`) + `Application::HandleCommand` case, then IPC handler + preload method. Every command is checked against the table first, so a command with no entry is rejected rather than run.

### Engine threading

`Application::Run` is a 2 ms polling loop that drains the command queue on the main thread. Around it:

- **Feeder thread** (`AudioDecoder::LaunchFeeder`) reads interleaved PCM into every registered `RingBuffer` by name. `Application::LoadAudio` registers `"Playback"` (5 s).
- **Audio callback thread** (miniaudio → `PlaybackService::OnAudioCallback`) pulls through `TimeProcessor` (RubberBand), applies volume, and emits a `position` event every 4th callback.
- **Analysis thread** (`AnalysisService::StartAnalysis`) runs while playing, sleeping `IntervalMs - executionTime`.
- **HTTP and WebSocket threads** from `main.cpp`.

`CommandQueue`/`MessageQueue` are mutex-guarded singletons and are the only sanctioned cross-thread channel — prefer pushing a message over reaching across services. Seeking follows the same rule: the command thread bumps an atomic generation on `AudioDecoder`, the feeder marks each `RingBuffer` with that generation, repositions itself and republishes it, and the audio callback then drops only what was written before the mark. Nothing sleeps waiting for another thread. `FeederState` belongs to the transport: the feeder never changes it itself, not even at end of file. A track ends when `TimeProcessor` reports it has drained, meaning the source is exhausted and RubberBand has been flushed with a final block, not when a frame counter reaches the file length.

`Application` owns the only copy of playback state, as a `TransportState` (`Empty → Loading → Ready ⇄ Playing ⇄ Paused`). `GET /status` is the one route that answers rather than queues: it reads atomics, so it is safe to call from the HTTP thread and doubles as a liveness probe. `engine/tests/smoke.ps1` drives a real engine through the sequences that used to crash it.

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

Vite + React 19 + Ant Design 6, Redux Toolkit for all shared state. Slices: `app` (file open, canvas width, status messages, engine connection), `playback` (duration, current time, waveform peaks), `analysis` (spectrum, notes, histograms, key, chords), `settings`.

`EngineEventRouter` is a render-less component mounted once in `App.jsx`; it is the **only** place engine signals become dispatches. That covers WebSocket events, the socket's own connection state (`useEngineConnection`) and the engine process status main pushes over `window.api.onEngineStatus`. Components read via `useSelector` and never touch the socket. `WebSocketProvider` wraps the Redux `Provider` in `main.jsx`.

The engine's health reaches the UI as two independent facts, both shown in the footer: `app.connectionState` is whether the socket is up (`connecting`/`connected`/`disconnected`, reconnecting with backoff), and `app.engineStatus` is what main knows about the process (`starting`/`ready`/`external`/`failed`/`exited`, with an error string). A spawn failure is worth showing while the socket is still retrying, which is why both exist.

Visualisation (`SpectrumCanvas`, `RollingNotesHeatMap`) is imperative 2D canvas drawn in `requestAnimationFrame`, not React elements — at ~5 ms analysis intervals, re-rendering per frame is not viable.

## Conventions and traps

- C++ uses tabs, Allman braces, `m_` members, `Adagio` namespace, PascalCase methods. Most analysis stages are header-only; services are `.h`/`.cpp` pairs.
- JS is 4-space, single quotes, arrow-function components, `.jsx` extension for *every* source file including slices, hooks, and utils.
- The WebSocket client lives in `app/ui/src/engine-client/`: `WebSocketEngine.jsx` (the socket, with reconnect backoff and a `CONNECTION_STATE`), `WebSocketContext.jsx` (the context alone) and `WebSocketProvider.jsx` (the component). The context and the provider are separate files because fast refresh wants a module to export components *or* values, not both.
- The engine binary lives in `app/engine-bin/`, which holds nothing else. Keep source out of it: electron-builder copies the whole folder into the packaged resources.
- `app/ui/src/store/PlaybackSlice.jsx` is capitalised on disk but imported as `./playbackSlice` everywhere. This only resolves on case-insensitive filesystems (Windows/macOS). Fix the filename if the build ever moves to Linux.
- `electron-builder.yml` copies `app/engine-bin/` to `resources/engine/` and `main.js` spawns `<resourcesPath>/engine/AdagioEngine.exe`, so the built engine binary must be dropped into `app/engine-bin/` before packaging. Its `directories.output` is `release`, not `dist`: electron-builder excludes its own output directory from the packaged files, and `app/dist/ui` is what Vite builds.
- The engine's stdin is spawned as a pipe, not `'ignore'`. The engine shuts down on stdin EOF, so main closes it on `will-quit` to free ports 5000 and 9001, and `'ignore'` would look like an immediate EOF.
- Every `ipcMain.handle` relay answers `{ok: true, value}` or `{ok: false, error}` and never throws, so a renderer call must check `ok` rather than rely on a rejection.
- Ports 5000 (HTTP) and 9001 (WebSocket) are hardcoded on both sides. `WSServer`'s constructor ignores its `port` argument and hardcodes 9001.
