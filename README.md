# Adagio

Adagio is a desktop app for listening to music closely. It plays an audio file at any speed from 20% to 100% without changing its pitch, and while it plays it shows the spectrum, the notes it hears, the key and the chords.

It is two programs. The window is an Electron and React app in `app/`. The audio work happens in a separate C++20 engine in `engine/`, which decodes the file, plays it through miniaudio, time-stretches it with RubberBand and runs the analysis. They talk over a local WebSocket, described once in `protocol/protocol.json`.

## Prerequisites

Adagio builds on Windows 10 or 11, x64.

- **Visual Studio 2022 or later** with the *Desktop development with C++* workload. The engine is C++20 and is compiled with `/arch:AVX2`, so it needs a CPU with AVX2 (Intel since 2013, AMD since 2015).
- **CMake 3.20 or later.**
- **Git.** CMake downloads every C++ dependency at a pinned tag on first configure: kfr, IXWebSocket, nlohmann/json, RubberBand and doctest. Nothing needs to be installed or copied in by hand.
- **Node.js 22 or later**, with npm.
- **PowerShell 7** (`pwsh`), to run the engine's socket tests.

## Build and run

Build the engine first:

```bash
cmake -S engine -B engine/build
cmake --build engine/build --config Release
```

The first configure takes a few minutes while the dependencies download.

Install the app's packages. `app/` holds Electron and `app/ui/` holds the React front end, and each has its own `package.json`:

```bash
cd app && npm install
cd ui && npm install
```

Run it in development, from `app/`:

```powershell
# Either let Electron start the engine...
$env:ADAGIO_ENGINE_PATH = '..\engine\build\Release\AdagioEngine.exe'
npm run dev
# ...or leave ADAGIO_ENGINE_PATH unset, start the engine yourself, then npm run dev.
```

This starts Vite on port 5173 and then Electron, with DevTools open. Without an engine the footer reads "Connecting…" and then "Disconnected".

## Package

Copy the engine into `app/engine-bin/`, then build from `app/`:

```bash
cp engine/build/Release/AdagioEngine.exe app/engine-bin/
cd app && npm run build
```

The installer and an unpacked build land in `app/release/`. The packaged app starts the engine it carries and waits for it before opening its window. `app/engine-bin/` is copied into the package whole, so keep anything but the engine out of it.

## Test

```bash
ctest --test-dir engine/build -C Release   # unit tests
pwsh engine/tests/smoke.ps1                # a real engine, driven over its socket
pwsh engine/tests/stress.ps1               # load, play and clear on a long track
cd app/ui && npm run lint
```

The smoke test covers the command sequences that used to crash the engine, the protocol and its authentication, playback to the end of a track at 50% and 100%, and the rate and size of what the engine sends while playing. Its playback cases need an audio output device.

In development the renderer logs its own frame timings to the DevTools console every 5 seconds. `localStorage.setItem('adagio:perf', 'off')` turns that off.

## Layout

```
app/electron/      Electron main process: file dialog, engine launch, per-launch token
app/ui/src/        React front end
  engine-client/   the WebSocket, binary frame decoding and the frame store
  router/          the one place engine events become state
  components/      the UI, with the spectrum and heat maps drawn on canvas
  store/           Redux slices
engine/src/
  Core/            application loop, transport state, WebSocket server, PCM feeder
  IO/              file loading, playback and the audio callback, waveform peaks
  Analysis/        the analysis pipeline and its stages
engine/tests/      unit tests, smoke.ps1 and stress.ps1
protocol/          protocol.json, read by both sides
```

`CLAUDE.md` describes the architecture in detail: the threads, the protocol, how analysis works and the traps worth knowing.

## Licence

The engine links RubberBand, which is licensed under the GPL (version 2 or later) unless you hold a commercial licence from its authors. A build of Adagio that you distribute is bound by the GPL as a result.
