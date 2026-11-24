"""Rescore every saved run against the current truth.json, grid.json and metric, without the engine. Use it after fixing
the transcription or the alignment, or changing the metric, so earlier runs stay comparable with later ones.

usage: python rescore.py [--work DIR]      writes runs/log.rescored.jsonl and prints old -> new totals
Runs made with --no-frames have nothing to rescore and are skipped.
"""
import argparse
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import workspace
from metric import Reference, score_frames


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--work', default='.')
    a = ap.parse_args()
    cfg = workspace.load(a.work)
    ref = Reference(cfg['grid'], cfg['truth'])
    rows = [json.loads(l) for l in open(cfg['runs'] / 'log.jsonl')]
    out = []
    for r in rows:
        path = cfg['runs'] / f"{r['iteration']:03d}.json"
        if not path.exists():
            print(f"{r['iteration']:3d} skipped (no frames)")
            out.append(r)
            continue
        new = score_frames(json.load(open(path)), ref)
        print(f"{r['iteration']:3d} {r['total']:.4f} -> {new['total']:.4f}  {r['name']}")
        out.append({**r, **{k: round(v, 4) for k, v in new.items()}})
    with open(cfg['runs'] / 'log.rescored.jsonl', 'w') as f:
        for r in out:
            f.write(json.dumps(r) + '\n')
    print('wrote runs/log.rescored.jsonl: replace log.jsonl with it once you have checked it')


if __name__ == '__main__':
    main()
