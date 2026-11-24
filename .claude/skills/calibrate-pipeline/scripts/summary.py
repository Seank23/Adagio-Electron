"""Progress so far: every run in runs/log.jsonl with the running best starred, or the top N.

usage: python summary.py [--work DIR] [--top N] [--best-out best.json]
"""
import argparse
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import workspace
from metric import notes_f1


def row_text(r, mark=''):
    return (f"{r['iteration']:3d} {r['total']:.3f}{mark:2s} F1 {notes_f1(r):.3f}  mel {r['melody']:.3f} harm {r['harmony']:.3f} "
            f"prec {r['precision']:.3f} chord {r['chord']:.3f} key {r['key']:.3f} n/f {r['notesPerFrame']:5.2f}  {r['name']}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--work', default='.')
    ap.add_argument('--top', type=int)
    ap.add_argument('--best-out')
    a = ap.parse_args()
    cfg = workspace.load(a.work)
    rows = [json.loads(l) for l in open(cfg['runs'] / 'log.jsonl')]
    if a.top:
        for r in sorted(rows, key=lambda r: -r['total'])[:a.top]:
            print(row_text(r))
    else:
        best = -1
        for r in rows:
            mark = ''
            if r['total'] > best:
                best, mark = r['total'], ' *'
            print(row_text(r, mark))
    top = max(rows, key=lambda r: r['total'])
    recent = rows[-15:]
    print(f"\nbest #{top['iteration']} {top['total']:.4f} ({top['name']}); "
          f"best of the last {len(recent)}: {max(r['total'] for r in recent):.4f}; {len(rows)} runs")
    if a.best_out:
        keep = {k: top[k] for k in ('pipeline', 'engine', 'a4', 'settings') if top.get(k) is not None}
        keep['name'] = f"B{top['iteration']}"
        out = a.best_out if os.path.isabs(a.best_out) else cfg['work'] / a.best_out
        json.dump(keep, open(out, 'w'), indent=1)
        print(f'best config -> {out}')


if __name__ == '__main__':
    main()
