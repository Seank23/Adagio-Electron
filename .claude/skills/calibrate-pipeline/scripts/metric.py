"""Score an Adagio run against the transcription (the heuristic and its reasons: references/metric.md).

Every analysis frame is compared with what the score says is sounding at that moment, located through grid.json.

Notes (70%)
  melody 30%    each melody note sounding through all of [t - 0.10, t] is among the frame's notes: exact MIDI 1.0,
                another octave 0.5 (1.0 in the truth's looseOctaveBars).
  harmony 20%   half the bass note (exact 1.0, another octave 0.5), half the share of the chart chord's pitch classes
                found; just the chord share where no bass note is sounding.
  precision 20% share of the frame's notes whose pitch class is a chord tone, or a melody or bass note, of anything
                sounding within [t - 0.30, t + 0.05].
Labels (30%)
  chord 20%     top predicted chord: 0.5 for the root (either note of a slash chord) + 0.5 x Jaccard of the pitch-class
                sets, best over the chart chords within [t - 0.30, t + 0.05].
  key 10%       the credit truth.json's "key" gives the detected key name ("Eb Major": 1.0, "C Minor": 0.5).

usage: python metric.py runs/NNN.json [--work DIR]      prints the totals and a per-bar breakdown
"""
import argparse
import bisect
import json
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import workspace
from truth import Truth, engine_chord

WEIGHTS = {'melody': 0.30, 'harmony': 0.20, 'precision': 0.20, 'chord': 0.20, 'key': 0.10}
MELODY_HOLD = 0.10            # a note must have sounded this long before the frame to be required
TOLERANCE = (-0.30, 0.05)     # what may still (or already) be sounding: release, chord-window lag and frame centring


class Reference:
    def __init__(self, grid_path, truth):
        self.truth = truth if isinstance(truth, Truth) else Truth(truth)
        self.grid = json.load(open(grid_path))
        self.times = [g['time'] for g in self.grid]
        self.start = self.grid[0]['time']
        self.end = self.grid[-1]['next']

    def locate(self, t):
        """(grid entry, beat within the bar) at time t, or None outside the bars."""
        if t is None or t < self.start or t >= self.end:
            return None
        i = bisect.bisect_right(self.times, t) - 1
        g = self.grid[i]
        return g, g['beat'] + (t - g['time']) / (g['next'] - g['time'])

    def notes_at(self, t, part):
        loc = self.locate(t)
        if loc is None:
            return []
        g, beat = loc
        return self.truth.sounding(g['scoreBar'] - 1, beat, part)

    def notes_through(self, t0, t1, part, step=0.02):
        """Notes sounding at every sample point of [t0, t1]: the same score note (recorded bar, onset and pitch)."""
        sets = []
        for t in np.arange(t0, t1 + 1e-9, step):
            loc = self.locate(t)
            if loc is None:
                return set()
            g, beat = loc
            bar = self.truth.bars[g['scoreBar'] - 1]
            sets.append({(g['audioBar'], b, m) for (b, d, m) in bar[part] if b <= beat < b + d})
        if not sets:
            return set()
        return {m for (_, _, m) in set.intersection(*sets)}

    def notes_within(self, t0, t1, part, step=0.02):
        out = set()
        for t in np.arange(t0, t1 + 1e-9, step):
            out.update(self.notes_at(t, part))
        return out

    def chords_within(self, t0, t1, step=0.02):
        out = set()
        for t in np.arange(t0, t1 + 1e-9, step):
            loc = self.locate(t)
            if loc is None:
                continue
            g, beat = loc
            c = self.truth.chord_at(g['scoreBar'] - 1, beat)
            if c:
                out.add(c)
        return out


def score_frames(frames, ref, detail=False):
    truth = ref.truth
    acc = {k: [] for k in WEIGHTS}
    per_bar = {}

    def add(bar, part, value):
        acc[part].append(value)
        per_bar.setdefault(bar, {}).setdefault(part, []).append(value)

    for f in frames:
        t = f['t']
        loc = ref.locate(t)
        if loc is None:
            continue
        g, beat = loc
        bar = g['audioBar']
        loose = bar in truth.loose_octave_bars
        midis = {n[0] for n in f['notes']}
        pcs = {m % 12 for m in midis}

        required = ref.notes_through(t - MELODY_HOLD, t, 'melody')
        if required:
            vals = [1.0 if m in midis else ((1.0 if loose else 0.5) if m % 12 in pcs else 0.0) for m in required]
            add(bar, 'melody', float(np.mean(vals)))

        chord = truth.chord_at(g['scoreBar'] - 1, beat)
        bass = ref.notes_through(t - MELODY_HOLD, t, 'bass')
        bass_score = float(np.mean([1.0 if m in midis else (0.5 if m % 12 in pcs else 0.0) for m in bass])) if bass else None
        if chord:
            cpcs = truth.chords[chord][1]
            cover = len(cpcs & pcs) / len(cpcs)
            add(bar, 'harmony', 0.5 * bass_score + 0.5 * cover if bass_score is not None else cover)
        elif bass_score is not None:
            add(bar, 'harmony', bass_score)

        window = (t + TOLERANCE[0], t + TOLERANCE[1])
        chart = ref.chords_within(*window)
        if midis:
            allowed = set()
            for c in chart:
                allowed |= truth.chords[c][1]
            allowed |= {m % 12 for m in ref.notes_within(*window, 'melody')}
            allowed |= {m % 12 for m in ref.notes_within(*window, 'bass')}
            add(bar, 'precision', sum(1 for m in midis if m % 12 in allowed) / len(midis))

        if chart:
            best = 0.0
            if f['chords']:
                root, epcs = engine_chord(f['chords'][0][0])
                for c in chart:
                    roots, ch = truth.chords[c]
                    jac = len(ch & epcs) / len(ch | epcs) if epcs else 0.0
                    best = max(best, 0.5 * (root in roots) + 0.5 * jac)
            add(bar, 'chord', best)

        acc['key'].append(truth.key_credit.get(f['key'], 0.0))

    parts = {k: float(np.mean(v)) if v else 0.0 for k, v in acc.items()}
    total = sum(WEIGHTS[k] * parts[k] for k in WEIGHTS)
    in_bars = [len(f['notes']) for f in frames if ref.locate(f['t'])]
    out = {'total': total, **parts, 'notesPerFrame': float(np.mean(in_bars)) if in_bars else 0.0}
    if detail:
        out['perBar'] = {b: {k: float(np.mean(v)) for k, v in d.items()} for b, d in sorted(per_bar.items())}
    return out


def notes_f1(r):
    """A notes-only check on the weighted total: recall from melody and harmony, against precision."""
    rec = (r['melody'] * WEIGHTS['melody'] + r['harmony'] * WEIGHTS['harmony']) / (WEIGHTS['melody'] + WEIGHTS['harmony'])
    p = r['precision']
    return 2 * rec * p / (rec + p) if rec + p else 0.0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('frames')
    ap.add_argument('--work', default='.')
    a = ap.parse_args()
    cfg = workspace.load(a.work)
    ref = Reference(cfg['grid'], cfg['truth'])
    r = score_frames(json.load(open(workspace.path(cfg, a.frames))), ref, detail=True)
    print(json.dumps({k: round(v, 4) for k, v in r.items() if k != 'perBar'}), f"F1 {notes_f1(r):.3f}")
    for b, d in r['perBar'].items():
        print(f'bar {b:3d}', ' '.join(f'{k} {v:.2f}' for k, v in d.items()))


if __name__ == '__main__':
    main()
