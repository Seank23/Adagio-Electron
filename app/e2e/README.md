# End-to-end validation

`validation.spec.js` automates the UI overhaul guide's §7 validation. Playwright launches
the real Electron app with the real engine, and the renderer is served by Vite.

```bash
cd app && npm run test:e2e
```

Requirements:

- A Release engine build at `engine/build/Release/AdagioEngine.exe`, or `ADAGIO_ENGINE_PATH`
  pointing at one.
- Port 9001 free. Each test spawns its own engine there, so the tests run one at a time.
- Nothing already on port 5173 other than this repo's Vite: the config reuses a running dev
  server.

Every launch gets its own `userData` directory through `ADAGIO_USER_DATA`, so one test's
`preferences.json` (a Light theme, a 16 kHz analysis) never reaches the next. The restart
test passes the first launch's directory back in.

The fixtures are WAV files generated at 48 kHz in a temp directory: a C major chord (40 s
and 1.5 s) and a 452.9 Hz tone. At 48 kHz one analysis hop is a whole number of samples at
every rate and hop `protocol.json` allows, so the step test reads the default hop from there
and each step moves the readout by exactly one hop (16 ms at 8 kHz and 128).

## What §7 item each test covers

| §7 item | Test |
|---|---|
| Cold start without the engine | cold start without the engine… |
| Open, play, pause, stop, skip back | open, play, pause, stop and skip back… |
| Reload mid-play | a reload mid-play brings back… (and the waveform comes back) |
| Speed | speed presets follow the engine… |
| Repeat | repeat restarts the track… |
| Step and reset | step: each step is one hop… |
| Timeline | timeline: the overview zooms and pans… |
| Spectrum and keyboard | switching to Linear… · the piano and the spectrum grid draw… · sidebar: the chord badge matches… |
| Sidebar | sidebar: … the tonic is marked, and the top bar fills 90% |
| Tuning | tuning: a tone at 452.9 Hz… |
| Empty and drop | empty state: Close brings the drop card back… |
| Window size | window size: nothing scrolls or overlaps… |
| Preferences | preferences: Ctrl+, and the cog open them… · preferences survive a restart… |

Also covered beyond §7: the engine starts at 20% volume, and a seek keeps the analysis.

## Still checked by hand

These need eyes, a second client, or a real drag from Explorer:

- **Both themes.** The suite runs in whatever theme the OS has, apart from the Preferences
  test that switches to Light. The palette and the "no colour literals" rule are unit tested,
  but how each theme looks isn't, and nor is the window frame or System following Windows.
- **A hand-started engine ignores the saved analysis values.** Only an engine main spawns
  gets `--sample-rate`, `--frame-length`, `--hop-size` and `--frame-smoothing`;
  the modal then shows the engine's own values.
- **A reset seen by two clients.** The engine broadcasts `analysisReset` to every client,
  which `smoke.ps1` checks. A second UI on the same spawned engine needs the token, so
  isn't automated here.
- **Dropping a real file.** A test can't give a dropped file a path on disk, because
  `webUtils` only knows files from the OS. The suite checks the refusal of an unsupported
  file; `useFileDrop` is unit tested for the rest.
- **Canvas details**: bars with no gaps at every zoom, a quiet ending staying quiet,
  cents colours on note tags, and dots matching tags. `tuningBand`, the axis, and peak
  picking are unit tested; the pixels aren't compared.
- **Follow keeping the playhead in view at 75%.**
- **Performance**: the `devPerf` log and the React Profiler over a minute of playback.
  The unit tests do check that the top bar and sidebar don't re-render on position
  updates, and that the sidebar doesn't re-render per analysis event.
