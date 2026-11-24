"""Write a batch of configs that vary a base config.

usage: python sweep.py BASE --vary Stage.KEY=v1,v2,... [--vary ...] [--grid] [-o batch.json]

BASE is a config file (as in a batch), "best" (the best run in runs/log.jsonl) or an iteration number from the log.
Each --vary is swept on its own from the base (one change at a time) unless --grid asks for every combination.
Special keys: a4=441,442 and engine.frameLength=4096. Values are read as JSON where they parse ("Scale Match" stays a
string; quote nothing). Names read "B<iteration> + KEY value", so the log says what each run changed from what.

examples:
    python sweep.py best --vary PeakExtractor.MIN_PROMINENCE=0.2,0.3,0.5 -o b20.json
    python sweep.py 53 --vary ChordPredictor.SCORE_THRESHOLD=0.05,0.07 --vary ChordPredictor.ROLLING_WINDOW=0.5,0.8 --grid
"""
import argparse
import copy
import itertools
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import workspace


def parse_value(s):
    try:
        return json.loads(s)
    except ValueError:
        return s


def base_config(spec, cfg):
    if spec not in ('best',) and not spec.isdigit():
        b = json.load(open(workspace.path(cfg, spec)))
        return (b[0] if isinstance(b, list) else b), b.get('name', 'base') if isinstance(b, dict) else 'base'
    rows = [json.loads(l) for l in open(cfg['runs'] / 'log.jsonl')]
    row = max(rows, key=lambda r: r['total']) if spec == 'best' else next(r for r in rows if r['iteration'] == int(spec))
    base = {k: row[k] for k in ('pipeline', 'engine', 'a4', 'settings') if row.get(k) is not None}
    return base, f"B{row['iteration']}"


def with_change(base, key, value):
    c = copy.deepcopy(base)
    if key == 'a4':
        c['a4'] = value
    elif key.startswith('engine.'):
        c.setdefault('engine', {})[key.split('.', 1)[1]] = value
    else:
        stage, k = key.split('.', 1)
        c.setdefault('settings', {}).setdefault(stage, {})[k] = value
    return c


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('base')
    ap.add_argument('--vary', action='append', default=[])
    ap.add_argument('--grid', action='store_true')
    ap.add_argument('--work', default='.')
    ap.add_argument('-o', '--out')
    a = ap.parse_args()
    cfg = workspace.load(a.work)
    base, label = base_config(a.base, cfg)
    axes = []
    for v in a.vary:
        key, values = v.split('=', 1)
        axes.append([(key, parse_value(x)) for x in values.split(',')])
    batch = []
    if a.grid:
        for combo in itertools.product(*axes):
            c = base
            for key, value in combo:
                c = with_change(c, key, value)
            c['name'] = f'{label} + ' + ', '.join(f'{k.split(".")[-1]} {v}' for k, v in combo)
            batch.append(c)
    else:
        for axis in axes:
            for key, value in axis:
                c = with_change(base, key, value)
                c['name'] = f'{label} + {key.split(".")[-1]} {value}'
                batch.append(c)
    if not batch:
        base['name'] = f'{label} (repeat)'
        batch = [base]
    text = json.dumps(batch, indent=1)
    if a.out:
        open(workspace.path(cfg, a.out) if not os.path.isabs(a.out) else a.out, 'w').write(text)
        print(f'{len(batch)} configs -> {a.out}')
    else:
        print(text)


if __name__ == '__main__':
    main()
