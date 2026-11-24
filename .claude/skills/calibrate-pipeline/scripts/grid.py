"""Build grid.json: the time of every recorded beat and which score bar and beat it plays.

Beats come from librosa's tracker, extended on the median beat interval to cover the whole form. Which tracked beat is
bar 1's downbeat is chosen by how well the truth's bass line agrees with the recording's bass chroma, eighth by eighth,
over every offset within --search beats of the first tracked beat. The best few are printed: a clear winner (0.05 or more
ahead) means the form and the bar count are right; a near tie means check them.

usage: python grid.py [--work DIR] [--bpm 103] [--search 8] [--start-index N]

Assumes a steady tempo (a band to a click, most studio pop). For rubato or tempo changes, beat-track in sections or
place the barlines by hand: grid.json is just a list of {audioBar, scoreBar, beat, time, next}.
"""
import argparse
import json
import os
import sys

import librosa
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import workspace
from truth import Truth

HOP = 256


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--work', default='.')
    ap.add_argument('--bpm', type=float, help="the sheet's tempo, as the tracker's starting guess")
    ap.add_argument('--search', type=int, default=8)
    ap.add_argument('--start-index', type=int, help='force the beat index of bar 1 (from an earlier printout)')
    a = ap.parse_args()
    cfg = workspace.load(a.work)
    truth = Truth(cfg['truth'])
    bpb = truth.beats_per_bar
    form = truth.form

    y, sr = librosa.load(str(cfg['wav']), sr=22050, mono=True)
    duration = len(y) / sr
    kw = {'start_bpm': a.bpm} if a.bpm else {}
    _, beats = librosa.beat.beat_track(y=y, sr=sr, hop_length=HOP, units='time', **kw)
    ibi = float(np.median(np.diff(beats)))
    print(f'tracked {len(beats)} beats, {60 / ibi:.1f} bpm, first at {beats[0]:.3f}s')
    if a.bpm and abs(60 / ibi - a.bpm) > 0.15 * a.bpm:
        print('  the tracker disagrees with --bpm: check for a half- or double-time lock')

    tuning = librosa.estimate_tuning(y=y, sr=sr)
    C = np.abs(librosa.cqt(y, sr=sr, hop_length=HOP, fmin=librosa.note_to_hz('A0'), n_bins=48, bins_per_octave=12, tuning=tuning))
    times = librosa.frames_to_time(np.arange(C.shape[1]), sr=sr, hop_length=HOP)
    bass_pc = np.zeros((12, C.shape[1]))
    for i in range(48):
        bass_pc[(i + 9) % 12] += C[i]                       # bin 0 is A0
    bass_pc /= np.maximum(bass_pc.sum(axis=0, keepdims=True), 1e-9)

    b = list(beats)
    while b[0] > -(a.search + 1) * ibi:
        b.insert(0, b[0] - ibi)
    while len(b) < len(form) * bpb + 2 * a.search + 2 or b[-1] < duration + 2 * ibi:
        b.append(b[-1] + ibi)
    b = np.array(b)

    def agreement(start):
        total, n = 0.0, 0
        for ab, sb in enumerate(form):
            for k in range(bpb * 2):                         # eighths
                beat = k * 0.5
                bi = start + ab * bpb + int(beat)
                if bi + 1 >= len(b):
                    continue
                t0 = b[bi] + (beat - int(beat)) * (b[bi + 1] - b[bi])
                t1 = t0 + 0.5 * (b[bi + 1] - b[bi])
                if t0 < 0 or t1 > times[-1]:
                    continue
                m = truth.sounding(sb - 1, beat, 'bass')
                if not m:
                    continue
                fr = (times >= t0) & (times < t1)
                total += bass_pc[min(m) % 12, fr].mean()
                n += 1
        return total / max(n, 1), n

    first = int(np.argmin(np.abs(b - beats[0])))
    if a.start_index is not None:
        start = a.start_index
    else:
        results = sorted(((*agreement(s), s) for s in range(max(0, first - a.search), first + a.search + 1)), reverse=True)
        for score, n, s in results[:5]:
            print(f'bar 1 at beat index {s} ({b[s]:.3f}s): bass agreement {score:.3f} over {n} eighths')
        start = results[0][2]
    if start + len(form) * bpb >= len(b):
        sys.exit(f'the form needs {len(form)} bars but the recording ends first: check "form"')
    grid = []
    for ab, sb in enumerate(form):
        for k in range(bpb):
            bi = start + ab * bpb + k
            grid.append(dict(audioBar=ab + 1, scoreBar=sb, beat=k, time=float(b[bi]), next=float(b[bi + 1])))
    json.dump(grid, open(cfg['grid'], 'w'), indent=1)
    print(f"wrote {cfg['grid']}: bar 1 at {grid[0]['time']:.3f}s, last bar ends {grid[-1]['next']:.3f}s of {duration:.1f}s")


if __name__ == '__main__':
    main()
