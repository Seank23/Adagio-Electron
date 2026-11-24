"""Why a run scores what it does: where the melody and bass go missing, which false notes it reports and what explains
them (a harmonic of a real note, or leakage a semitone beside one), how each chart chord gets named, which keys it
settles on.

A false note is one whose pitch class nothing in the score allows at that moment (the metric's precision). It is put
down to harmonic h when it lies 12, 19, 24, 28 or 31 semitones (harmonics 2-6) above a real note: a reported note the
score allows, or a melody or bass note sounding. Harmonic 2 is an octave, so it is never false; 3 and 5 are the ones to
look for. Many 3rd harmonics mean the pipeline needs harmonic suppression, which no threshold provides.

usage: python diagnose.py runs/NNN.json [--work DIR]
"""
import argparse
import collections
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import workspace
from metric import Reference, TOLERANCE, MELODY_HOLD
from truth import note_name

HARMONICS = [(3, 19), (4, 24), (5, 28), (6, 31)]   # (harmonic, semitones above its fundamental)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('frames')
    ap.add_argument('--work', default='.')
    a = ap.parse_args()
    cfg = workspace.load(a.work)
    ref = Reference(cfg['grid'], cfg['truth'])
    truth = ref.truth
    frames = json.load(open(workspace.path(cfg, a.frames)))

    mel, bass, octs, keys = (collections.Counter() for _ in range(4))
    false_note, cause = collections.Counter(), collections.Counter()
    named = collections.defaultdict(collections.Counter)
    for f in frames:
        t = f['t']
        loc = ref.locate(t)
        if not loc:
            continue
        g, beat = loc
        midis = {n[0] for n in f['notes']}
        pcs = {m % 12 for m in midis}
        for m in midis:
            octs[m // 12 - 1] += 1
        if g['audioBar'] not in truth.loose_octave_bars:
            for m in ref.notes_through(t - MELODY_HOLD, t, 'melody'):
                if m in midis:
                    mel['exact'] += 1
                elif m % 12 in pcs:
                    mel['other octave'] += 1
                elif any(abs(x - m) == 1 for x in midis):
                    mel['a semitone off'] += 1
                else:
                    mel['missing'] += 1
        for m in ref.notes_through(t - MELODY_HOLD, t, 'bass'):
            bass['exact' if m in midis else ('other octave' if m % 12 in pcs else 'missing')] += 1
        chord = truth.chord_at(g['scoreBar'] - 1, beat)
        window = (t + TOLERANCE[0], t + TOLERANCE[1])
        allowed = set()
        for c in ref.chords_within(*window):
            allowed |= truth.chords[c][1]
        allowed |= {m % 12 for m in ref.notes_within(*window, 'melody')}
        allowed |= {m % 12 for m in ref.notes_within(*window, 'bass')}
        real = ({x for x in midis if x % 12 in allowed} | ref.notes_within(*window, 'melody')
                | ref.notes_within(*window, 'bass'))
        for m in midis:
            if m % 12 not in allowed:
                false_note[note_name(m)] += 1
                why = next((f'harmonic {h}' for h, semis in HARMONICS if m - semis in real), None)
                if why is None and any(abs(m - x) == 1 for x in real):
                    why = 'a semitone from a real note'
                cause[why or 'unexplained'] += 1
        if chord:
            named[chord][f['chords'][0][0] if f['chords'] else '-'] += 1
        keys[f['key'] or '-'] += 1

    def share(c):
        total = sum(c.values()) or 1
        return ', '.join(f'{k} {v / total:.0%}' for k, v in c.most_common())

    print('melody, outside the loose-octave bars:', share(mel))
    print('bass:', share(bass))
    print('octaves reported:', ' '.join(f'{o}:{n}' for o, n in sorted(octs.items())))
    print('false notes, most common:', ', '.join(f'{k} {v}' for k, v in false_note.most_common(15)))
    print('false notes explained as:', share(cause))
    print('keys:', share(keys))
    print('chart chord -> top predicted chord:')
    for c, cnt in named.items():
        print(f'  {c:9s} ' + ', '.join(f'{e} {n / sum(cnt.values()):.0%}' for e, n in cnt.most_common(5)))


if __name__ == '__main__':
    main()
