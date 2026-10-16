# Adagio

Adagio is a Windows desktop app esigned to provide a technical analysis of the musical elements present in an audio track. It slows an audio file down to as little as 20% of its speed without changing its pitch, and while it plays it shows the spectrum, the notes it hears, the key and the chords. While the analysis produced is not guaranteed to be completely accurate, Adagio aims to serve as a tool to help musicians and music enthusiasts in their quest to transcribe, or better understand a piece of music.

It is the spiritual successor to [MusicAnalyser](https://github.com/Seank23/MusicAnalyser), rebuilt from scratch: see [Improvements over MusicAnalyser](#improvements-over-musicanalyser).

<img width="2377" height="1332" alt="image" src="https://github.com/user-attachments/assets/ddb111f9-f2f3-494f-b2fb-788b0b017e0d" />

## Features

- **Open** a WAV, MP3 or FLAC file from the top bar or with Ctrl+O, or drop one on the window.
- **Play** at any speed from 20% to 100%, with presets at 50%, 75% and 100%, at the original pitch. The transport bar also holds the volume, repeat and follow; play, back to start, stop and close are in the top bar.
- **Timeline**: the track's waveform, a time ruler and an overview of the whole track. Click the waveform to seek. On the overview, press and drag up or down to zoom around that point and sideways to pan, or use the zoom and fit buttons. Follow keeps the playhead in view.
- **Spectrum**: the live spectrum on a log frequency axis, with detected notes annotated on top and a badge showing the most likely chord anchored in the top right. Show it linear or in decibels, and average it over 1 to 10 frames.
- **Note activity**: a piano keyboard lined up under the spectrum, lit by the notes heard over the last half second.
- **Key and chords**: the sidebar shows the detected key with how strongly each pitch class has sounded, weighted towards the last 20 seconds, and the most likely chords with their probabilities.
- **Tuning**: set the A4 reference from 415 to 466 Hz for a recording that isn't at 440. Drag the readout up or down, or use its − and + buttons (Shift for finer steps); double-click puts it back to 440. Beside it is the average tuning error of the notes heard, in cents. Each track opens at 440.
- **Step** one analysis frame at a time while paused or stopped, with `.` or by holding the step button, and reset the analysis to start the key and chords afresh.
- **Preferences** (Ctrl+,): a System, Light or Dark theme; the analysis sample rate (4, 8 or 16 kHz), frame size (2048, 4096 or 8192) and hop size (32 to 256 samples), which trade frequency detail against time detail; the spectrum's smoothing; and About.
- **Status bar**: the analysis rate and settings, the time the pipeline takes per frame (on hover), and whether the engine is connected.

### Keyboard

| Key | Action |
|---|---|
| Space | Play or pause |
| ← / → | Seek back or forward 5 seconds |
| `.` | Step one analysis frame, while paused or stopped |
| Ctrl+O | Open a file |
| Ctrl+, | Preferences |

## Install

Download `Adagio Setup <version>.exe` from the [releases page](https://github.com/Seank23/Adagio-Electron/releases) and run it. It installs for the current user, with no administrator prompt, and adds Start menu and desktop shortcuts; uninstall it from Settings > Apps. The installer is not code-signed, so SmartScreen warns before it runs: choose **More info**, then **Run anyway**.

Adagio needs Windows 10 or 11 on x64, a CPU with AVX2 (Intel since 2013, AMD since 2015) and an audio output device. It keeps its preferences in `%APPDATA%\Adagio\preferences.json`.

## How it works

Adagio is two programs. The window is an Electron and React app in `app/`. The audio work happens in a separate C++20 engine in `engine/`, which decodes the file, plays it through miniaudio, time-stretches it with RubberBand and runs the analysis. They talk over a local, authenticated WebSocket, described once in `protocol/protocol.json`.

The engine analyses its own copy of the track, mixed to mono and resampled to the analysis sample rate. Each frame goes through an FFT, a harmonic product spectrum, peak picking, note detection against the A4 reference, a key detector and a chord predictor. Frames arrive at a fixed rate in wall time, so slower playback analyses the music more finely. The spectrum streams to the window as binary frames every hop; the notes, key and chords as JSON about 31 times a second.

`CLAUDE.md` describes the architecture in detail: the threads, the protocol, how the analysis works and the traps worth knowing.

## Improvements over MusicAnalyser

MusicAnalyser was a C# WinForms app on .NET Framework that did the same job: play a track slowed down and show its spectrum, notes, key and chords. Adagio keeps the idea and the shape of its analysis chain (FFT, harmonic product spectrum, spectrum filter, peak picking, then notes, key and chords) and rebuilds everything around it.

- **A native engine in its own process.** MusicAnalyser ran its analysis from a timer on the window's UI thread, so a slow frame held up the interface and a busy interface held up the analysis. Adagio's C++ engine plays, time-stretches and analyses on threads of its own, and the window only draws what the engine sends it.
- **Analysis at a steady rate.** MusicAnalyser's timer stretched its interval to however long recent frames had taken, so the frames a second depended on the machine and the settings. Adagio analyses a frame each time the playhead moves one hop: 62.5 frames a second at the defaults and up to 500, each read centred on where the playhead is at that moment. At slower speeds it samples the music more finely.
- **Key and chords that don't depend on the frame rate.** MusicAnalyser took the key from its last 10,000 detected notes and the chords from note counts, so how far back they looked changed with the update rate, the playback speed and how busy the music was. Adagio weights everything it accumulates by the stretch of music each frame covers: the key leans on the last 20 seconds and the chords on the last half second, at any speed, hop or sample rate.
- **Analysis resolution you can choose.** MusicAnalyser's frame was fixed at 4,096 samples. Adagio's analysis sample rate (4, 8 or 16 kHz), frame size (2,048 to 8,192 samples) and hop (32 to 256 samples) are preferences you can change mid-track, to trade frequency detail against time detail for the music in hand.
- **A spectrum laid out like a keyboard.** MusicAnalyser plotted the spectrum on a linear frequency axis. Adagio's axis is logarithmic and gridded by piano key, with a keyboard beneath whose keys sit under the peaks; detected notes are dotted on it, and the magnitude can be linear or in decibels.
- **A full semitone of tuning.** MusicAnalyser's Pitch Sync moved note naming by up to 50 cents either way. Adagio sets the A4 reference anywhere from 415 to 466 Hz, a semitone each side of 440, so a recording at baroque pitch is named correctly.
- **Playback.** RubberBand does the time-stretching in place of SoundTouch, still down to 20% at the original pitch, and FLAC opens alongside WAV and MP3.
- **Installs like an app.** A per-user installer with Start menu shortcuts and an uninstaller replaces the zip to extract. The engine links its runtime statically, so nothing else needs installing.
- **Builds from a clean checkout, and is tested.** Every dependency comes at a pinned version through CMake or npm, with nothing to copy in by hand. Engine unit tests, socket-level smoke and stress tests, UI component tests and Playwright tests of the real app cover it, and CI builds the engine and runs its unit and smoke tests on every push.

Adagio is still work in progress so not everything has come across yet. MusicAnalyser's recording from an input device, spectrogram viewer, editable script chain and its presets (the CQT one among them), note highlight filter, pitch shifting and section looping have no equivalent in Adagio so far. These features will be added in future releases.

## Build from source

### Prerequisites

Adagio builds on Windows 10 or 11, x64.

- **Visual Studio 2022 or later** with the *Desktop development with C++* workload. The engine is C++20 and is compiled with `/arch:AVX2`.
- **CMake 3.20 or later.**
- **Git.** CMake downloads every C++ dependency at a pinned tag on first configure: kfr, IXWebSocket, nlohmann/json, RubberBand and doctest. Nothing needs to be installed or copied in by hand.
- **Node.js 22 or later**, with npm.
- **PowerShell 7** (`pwsh`), to run the engine's socket tests.

### Build and run

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

### Package

With the engine configured as above, build from `app/`:

```bash
cd app && npm run build
```

That rebuilds the Release engine and copies it into `app/engine-bin/` (`npm run build:engine` does that step alone), builds the UI, then packages both. The installer, `Adagio Setup <version>.exe`, and an unpacked build land in `app/release/`; the version is `app/package.json`'s. The installer is the only file a release needs. The packaged app starts the engine it carries and waits for it before opening its window. `app/engine-bin/` is copied into the package whole, so keep anything but the engine out of it.

The engine links the C++ runtime statically, so the installed app needs no Visual C++ Redistributable. Its icon, `asset/Adagio_Icon.ico`, is `asset/Adagio_Logo.png` redrawn at 16 to 256 px.

### Test

```bash
ctest --test-dir engine/build -C Release   # engine unit tests
pwsh engine/tests/smoke.ps1                # a real engine, driven over its socket
pwsh engine/tests/stress.ps1               # load, play and clear on a long track
cd app/ui && npm test                      # UI unit and component tests (Vitest, jsdom)
cd app/ui && npm run lint
cd app && npm run test:e2e                 # the real app and engine, driven by Playwright
```

The smoke test covers the command sequences that used to crash the engine, the protocol and its authentication, playback to the end of a track at 50% and 100%, and the rate and size of what the engine sends while playing. Its playback cases need an audio output device. The end-to-end tests need a Release engine build and port 9001 free; `app/e2e/README.md` has the details.

In development the renderer logs its own frame timings to the DevTools console every 5 seconds. `localStorage.setItem('adagio:perf', 'off')` turns that off.

## Layout

```
app/electron/      Electron main process: window, engine launch, per-launch token, preferences
app/ui/src/        React front end
  engine-client/   the WebSocket, binary frame decoding and the frame store
  router/          the one place engine events become state
  components/      the UI, with the spectrum, keyboard and timeline drawn on canvas
  store/           Redux slices
  theme/           the light and dark palettes
app/ui/test/       Vitest unit and component tests
app/e2e/           Playwright end-to-end tests
engine/src/
  Core/            application loop, transport state, WebSocket server, PCM feeder
  IO/              file loading, playback and the audio callback, waveform peaks
  Analysis/        the analysis pipeline and its stages
  Buffers/         the ring buffers between threads
engine/tests/      unit tests, smoke.ps1 and stress.ps1
protocol/          protocol.json, read by both sides
asset/             the logo and the app icon
```

## Licence

Adagio is free software under the GNU General Public License, version 3 or (at your option) any later version: see `LICENSE`. The engine links RubberBand and kfr, both under the GPL version 2 or later, so a distributed build is bound by the GPL in any case.

The installer puts the licence beside `Adagio.exe` as `LICENSE.txt`, with `THIRD-PARTY-NOTICES.txt`: the licences of everything else Adagio ships. The engine's part is `engine/THIRD-PARTY-NOTICES.txt`, kept by hand, so update it when a `FetchContent` tag changes or a library is added. The UI build appends a notice for every npm package in the bundle (`app/ui/scripts/thirdPartyNotices.js`), so those stay current on their own.
