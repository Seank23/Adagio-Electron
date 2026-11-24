"""Estimate the recording's tuning and the A4 to set for it. Records are often a few cents off 440, and NoteDetector's
cents limit then rejects notes that are really in tune. A4 is per track, so every run sets it (the "a4" of a config).

usage: python tuning.py [--work DIR]
"""
import argparse
import os
import sys

import librosa
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import workspace


def a4(cents):
    return 440.0 * 2 ** (cents / 1200)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--work', default='.')
    a = ap.parse_args()
    cfg = workspace.load(a.work)
    y, sr = librosa.load(str(cfg['wav']), sr=22050, mono=True)
    whole = 100 * librosa.estimate_tuning(y=y, sr=sr, bins_per_octave=12)
    print(f'estimate_tuning, whole mix: {whole:+.1f} cents -> A4 {a4(whole):.1f}')
    f0, voiced, prob = librosa.pyin(y, fmin=80, fmax=1000, sr=sr, frame_length=2048, hop_length=256)
    f = f0[voiced & (prob > 0.6)]
    if len(f):
        dev = ((1200 * np.log2(f / 440.0) + 50) % 100) - 50
        med = float(np.median(dev))
        print(f'pyin, the most prominent line ({len(f)} voiced frames): median {med:+.1f} cents -> A4 {a4(med):.1f}')
        h, e = np.histogram(dev, bins=10, range=(-50, 50))
        print('  ' + '  '.join(f'{lo:+.0f}:{c}' for c, lo in zip(h, e)))
    print('Sweep A4 over the estimate and +-1 Hz: the metric settles it (441 and 442 were 0.001 apart on the first track).')


if __name__ == '__main__':
    main()
