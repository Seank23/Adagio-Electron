"""Check the transcription against the recording, eighth by eighth, before trusting it: the strongest notes a reference
CQT (librosa, tuned to the recording) hears beside what truth.json says is sounding. Use it on any bar where the run
scores badly, to tell a transcription or alignment mistake (the reference CQT disagrees with the truth too) from an
engine one (only Adagio disagrees). It is how the first track's intro turned out to be recorded an octave above the sheet.

usage: python inspect_audio.py BARS [--work DIR]       BARS: recorded bar numbers, "9,10,11" or "9-12"
"""
import argparse
import json
import os
import sys

import librosa
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import workspace
from truth import Truth, note_name

HOP = 256
LOW = 36     # C2: bins below G3 are listed as "lo", the rest as "hi"
SPLIT = 55


def bar_list(s):
    out = []
    for part in s.split(','):
        lo, _, hi = part.partition('-')
        out += list(range(int(lo), int(hi or lo) + 1))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('bars', type=bar_list)
    ap.add_argument('--work', default='.')
    a = ap.parse_args()
    cfg = workspace.load(a.work)
    truth = Truth(cfg['truth'])
    grid = json.load(open(cfg['grid']))
    y, sr = librosa.load(str(cfg['wav']), sr=22050, mono=True)
    tuning = librosa.estimate_tuning(y=y, sr=sr)
    C = np.abs(librosa.cqt(y, sr=sr, hop_length=HOP, fmin=librosa.midi_to_hz(LOW), n_bins=60, bins_per_octave=12, tuning=tuning))
    times = librosa.frames_to_time(np.arange(C.shape[1]), sr=sr, hop_length=HOP)
    names = lambda ms: ','.join(note_name(m) for m in ms) or '-'
    for g in grid:
        if g['audioBar'] not in a.bars:
            continue
        for half in (0, 0.5):
            t0 = g['time'] + half * (g['next'] - g['time'])
            t1 = t0 + 0.5 * (g['next'] - g['time'])
            v = C[:, (times >= t0 + 0.03) & (times < t1)].mean(axis=1)
            split = SPLIT - LOW
            hi = np.argsort(-v[split:])[:5] + SPLIT
            lo = np.argsort(-v[:split])[:2] + LOW
            sb, beat = g['scoreBar'] - 1, g['beat'] + half
            print(f"bar {g['audioBar']:2d} (score {g['scoreBar']:2d}) beat {beat + 1:3.1f} {t0:6.2f}s  "
                  f"chord {str(truth.chord_at(sb, beat)):8s} melody {names(truth.sounding(sb, beat, 'melody')):10s} "
                  f"bass {names(truth.sounding(sb, beat, 'bass')):8s} | heard hi {names(hi)}  lo {names(lo)}")


if __name__ == '__main__':
    main()
