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
cd app/ui && npm run lint      # eslint flat config; the UI has no tests

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

Engine (CMake + MSVC; Windows-first — `/arch:AVX2`, and RubberBand is built via `msbuild`):

```bash
cmake -S engine -B engine/build
cmake --build engine/build --config Release
```

The wire protocol lives in `protocol/protocol.json`. Editing it re-runs CMake's configure step, which regenerates `engine/build/generated/Protocol.generated.h`; the UI imports the same file through the `@protocol` alias, which `vite.config.js` also has to allow in `server.fs`.

`engine/external/rubberband` is gitignored and `.gitmodules` is empty — RubberBand must be placed there manually before the engine will configure. kfr, IXWebSocket, and nlohmann/json are pulled by `FetchContent`.

**In dev mode the Electron main process spawns the engine only if `ADAGIO_ENGINE_PATH` points at a binary.** Without it, start `AdagioEngine.exe` yourself before `npm run dev`, or the UI will have no backend on port 9001 — the footer will read "Connecting…" and then "Disconnected". A packaged build always spawns the engine from `process.resourcesPath/engine/` and waits for its `Adagio engine ready` line before opening the window.

An engine main did not start has no token and requires none, so a hand-started one still accepts the renderer. One main spawned is given `--token=<hex>` and refuses anything that cannot present it.

## Architecture

### Three processes, one channel

```
React renderer ═══ WebSocket :9001 (authenticated) ═══> C++ engine
      │   commands  {id, cmd, args}  ───────────────────────>│
      │   replies   {type:"reply", id, ok, value|error}  <───│
      │   events    {type, value}                        <───│
      └── window.api (IPC) ──> Electron main: file dialog, engine token, spawn status
```

- **Commands and everything the UI observes share one socket.** The renderer sends `{id, cmd, args}`; `main.cpp` parses it on the connection's thread, pushes a `Command` onto the `CommandQueue` singleton, and the command thread answers with a `reply` addressed to the client that asked. Engine code anywhere pushes JSON onto the `MessageQueue` singleton; `WSServer::ProcessQueue` drains it every 5 ms, broadcasting or sending to one client.
- **The protocol is described once, in `protocol/protocol.json`.** CMake turns it into `Protocol.generated.h` (command enum, wire names, argument kinds, event names, limits) at configure time; `app/ui/src/utils/protocol.jsx` imports the same file through the `@protocol` alias. Neither side spells a command or event name by hand.
- **Adding an engine→UI event**: add it to `events` in `protocol.json`, push it with `Protocol::Event::<Name>`, and handle it in `app/ui/src/router/EngineEventRouter.jsx`.
- **Adding a UI→engine command**: add it to `commands` in `protocol.json` (which gives you the `CommandType` entry and the parser's argument check), then a row in the `TransportState` table (`engine/src/Core/TransportState.cpp`), an `Application::HandleCommand` case, and a method in `app/ui/src/engine-client/EngineCommands.jsx`. Every command is checked against the table first, so a command with no entry is rejected rather than run.
- **Authentication.** Main generates one token per launch, passes it to the engine on the command line and to the renderer through preload; the renderer puts it in the handshake query. The engine also refuses any handshake whose `Origin` is not in `protocol.json`'s `allowedOrigins`, which is what keeps a page in a browser off the channel whether or not a token is set.

### Engine threading

`Application::Run` is a 2 ms polling loop that drains the command queue on the main thread. Around it:

- **Feeder thread** (`AudioDecoder::LaunchFeeder`) reads interleaved PCM into every registered `RingBuffer` by name. `Application::LoadAudio` registers `"Playback"` (5 s).
- **Audio callback thread** (miniaudio → `PlaybackService::OnAudioCallback`) pulls through `TimeProcessor` (RubberBand), applies volume, and emits a `position` event every 4th callback.
- **Analysis thread** (`AnalysisService::StartAnalysis`) runs while playing and analyses a frame each time the playhead has moved `HopSize × speed` analysis-stream samples, so frames arrive at a fixed rate in wall time (31.25 Hz) and slower playback samples the source more finely.
- **WebSocket threads**: one per connection (where inbound frames are parsed) plus the queue thread that drains `MessageQueue`. A stdin thread queues `Shutdown` on EOF.

`CommandQueue`/`MessageQueue` are mutex-guarded singletons and are the only sanctioned cross-thread channel — prefer pushing a message over reaching across services. Seeking follows the same rule: the command thread bumps an atomic generation on `AudioDecoder`, the feeder marks each `RingBuffer` with that generation, repositions itself and republishes it, and the audio callback then drops only what was written before the mark. Nothing sleeps waiting for another thread. `FeederState` belongs to the transport: the feeder never changes it itself, not even at end of file. A track ends when `TimeProcessor` reports it has drained, meaning the source is exhausted and RubberBand has been flushed with a final block, not when a frame counter reaches the file length.

`Application` owns the only copy of playback state, as a `TransportState` (`Empty → Loading → Ready ⇄ Playing ⇄ Paused`), and broadcasts a `transport` event (state, position, duration, speed, volume) after every command that changes it. The UI renders from that and holds no playback state of its own. The `status` command answers the same payload in its reply without broadcasting, so it is both the liveness probe and the way a client that has just connected learns where the engine is. `engine/tests/smoke.ps1` drives a real engine over the socket through the sequences that used to crash it, plus the protocol and authentication rules.

### Analysis pipeline

`AnalysisService` keeps its **own** preprocessed stream, independent of playback: `PreprocessStream` mixes to mono, Butterworth-lowpasses, and resamples to `AnalysisParams.SampleRate` (currently `{8000 Hz, 4096}`, set in `Application::LoadAudio`), then loads it whole into a `RingBuffer`. `EstimatePlayhead` extrapolates the current playhead as `decoder.GetPlaybackTime() + (now - lastPlaybackFrameTimestamp) × speed`, and `ProcessFrameAt` reads a frame centred on it — so analysis is positioned by timestamp, not by consuming a stream. Each frame carries `DeltaTime`, the source seconds it covers (one hop's worth for an on-demand frame while paused).

Stages run in order in `AnalysisPipeline`, each mutating a shared `AnalysisContext`:

`FFTProcessor → HPSDownsamplerProcessor → SpectrumFilterProcessor → PeakExtractor → NoteDetector → KeyDetector → ChordPredictor`

- A stage is a header-only subclass of `AnalysisStage` in `engine/src/Analysis/`, overriding `Execute`, `GetType`, and `GetSettings`. Register it with `m_Pipeline->AddStage(...)` in `AnalysisService::BuildPipeline`, which runs once in the service's constructor: the pipeline and its settings outlive the loaded file, so `getAnalysisSchema` answers with nothing open.
- `GetSettings()` returns a JSON *schema* (`name`/`type`/`default`, plus `options` or `min`/`max`). `AddStage` harvests the defaults into the pipeline's settings map, keyed by `GetName()` — which is derived from `typeid(*this).name()`, so **the class name is the settings key**. Read values inside `Execute` via `GetSetting<T>(settings, "KEY")`; never read `context->Settings` directly.
- Call `AnalysisStage::Execute(context)` first in any override — it primes the settings definition that `GetSetting` depends on.
- `AnalysisContext::PersistentData` (the key's decayed accumulators, the chord window's frames, previous chord) survives across frames and is cleared on seek; everything else is per-frame.
- Anything accumulated across frames is weighted by `Frame.DeltaTime`, never counted per frame, so it doesn't change with the hop or the playback speed. The key decays as `H ← H·e^(−dt/τ) + score·dt` with `τ = ROLLING_WINDOW / 3`; the chord window is a hard `ROLLING_WINDOW` of source time, and a root's presence is in seconds.
- Results are serialised by `AnalysisPipeline::GetResultJson` into one `analysis` event. Adding a field there means adding it to `analysisSlice.jsx` too.
- `getAnalysisSchema` returns every stage's schema in pipeline order with the value in force; `setAnalysisSetting` checks a value against that same schema and applies it between frames. `ProcessFrame` takes one snapshot of the settings per frame, so a change never lands mid-frame. Note `GetSettings()` returns by value: bind it to a named local before iterating `.items()`, or the object is destroyed before the loop runs.

### UI

Vite + React 19 + Ant Design 6, Redux Toolkit for all shared state. Slices: `app` (canvas width, status messages, engine connection), `playback` (the engine's transport — state, speed, volume, duration — plus current time and waveform peaks), `analysis` (spectrum, notes, histograms, key, chords), `settings`.

Nothing is set optimistically. `playbackSlice` exports `selectIsPlaying`, `selectIsPaused` and `selectIsFileOpen`, all derived from the one transport state, so the UI cannot drift from the engine the way it did when it guessed. Components send commands through `useEngineCommands()`, which answers `{ok, value, error}` once the engine has run the command.

`EngineEventRouter` is a render-less component mounted once in `App.jsx`; it is the **only** place engine signals become dispatches. That covers WebSocket events, the socket's own connection state (`useEngineConnection`) and the engine process status main pushes over `window.api.onEngineStatus`. Components read via `useSelector` and never touch the socket. `WebSocketProvider` wraps the Redux `Provider` in `main.jsx`.

The engine's health reaches the UI as two independent facts, both shown in the footer: `app.connectionState` is whether the socket is up (`connecting`/`connected`/`disconnected`, reconnecting with backoff), and `app.engineStatus` is what main knows about the process (`starting`/`ready`/`external`/`failed`/`exited`, with an error string). A spawn failure is worth showing while the socket is still retrying, which is why both exist.

Visualisation (`SpectrumCanvas`, `RollingNotesHeatMap`) is imperative 2D canvas drawn in `requestAnimationFrame`, not React elements — at ~5 ms analysis intervals, re-rendering per frame is not viable.

## Conventions and traps

- C++ uses tabs, Allman braces, `m_` members, `Adagio` namespace, PascalCase methods. Most analysis stages are header-only; services are `.h`/`.cpp` pairs.
- JS is 4-space, single quotes, arrow-function components, `.jsx` extension for *every* source file including slices, hooks, and utils.
- The WebSocket client lives in `app/ui/src/engine-client/`: `WebSocketEngine.jsx` (the socket, with reconnect backoff, a `CONNECTION_STATE` and the pending-reply map behind `request()`), `WebSocketContext.jsx` (the context alone), `WebSocketProvider.jsx` (the component, which fetches the token before opening) and `EngineCommands.jsx` (one method per command). The context and the provider are separate files because fast refresh wants a module to export components *or* values, not both.
- Volume and speed travel normalised (0–1, and 0.2–2.0 from `protocol.json`'s `limits`). Percentages exist only in the sliders, which convert at the boundary in `PlaybackControls`.
- The engine binary lives in `app/engine-bin/`, which holds nothing else. Keep source out of it: electron-builder copies the whole folder into the packaged resources.
- `app/ui/src/store/PlaybackSlice.jsx` is capitalised on disk but imported as `./playbackSlice` everywhere. This only resolves on case-insensitive filesystems (Windows/macOS). Fix the filename if the build ever moves to Linux.
- `electron-builder.yml` copies `app/engine-bin/` to `resources/engine/` and `main.js` spawns `<resourcesPath>/engine/AdagioEngine.exe`, so the built engine binary must be dropped into `app/engine-bin/` before packaging. Its `directories.output` is `release`, not `dist`: electron-builder excludes its own output directory from the packaged files, and `app/dist/ui` is what Vite builds.
- **The WebSocket server's ping interval has to stay non-zero** (`WSServer::PingIntervalSeconds`). It is what bounds the connection thread's poll: at the library default the poll waits forever, and a wake-up missed while a large message is written from another thread parks that connection permanently — the socket stays open, the engine stays healthy, events keep queueing, and nothing the client sends is read again. It reproduces on any track long enough for the waveform event to need several of the library's 32 KB fragments, and only in Release, where the timing is tight enough. `engine/tests/stress.ps1` is the reproducer.
- `WSServer` never sends while holding its client lock: it copies the `shared_ptr`s out and writes outside it, because the inbound path needs that same lock to check a frame came from a client that was accepted.
- The engine's stdin is spawned as a pipe, not `'ignore'`. The engine shuts down on stdin EOF, so main closes it on `will-quit` to free port 9001, and `'ignore'` would look like an immediate EOF.
- `window.api` is down to what only main can do: `selectAudioFile`, `getEngineToken` and `onEngineStatus`. Its handlers answer `{ok, value}`/`{ok, error}` or a plain value and never throw. `engine.request()` follows the same contract, so a caller checks `ok` rather than relying on a rejection; a dropped socket answers every command in flight instead of leaving it hanging.
- The port lives in `protocol.json` and reaches both sides from there — `Protocol::Port` in C++, `ENGINE_WS_URL` in the UI. Nothing hardcodes 9001 any more.
