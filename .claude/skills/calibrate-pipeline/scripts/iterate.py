"""Run a batch of configurations through the real engine, score each one and log it.

usage: python iterate.py batch.json [--work DIR] [--no-frames]

batch.json is a list of configs (or one):
    {"name": "B53 + prom 0.2",                     shows in the log; name the base it varies ("B53" = iteration 53)
     "pipeline": "constantQ",                      optional, else calibration.json's
     "engine": {"frameLength": 4096},              optional, merged over calibration.json's; a change restarts the engine
     "a4": 441,                                    optional, A4 for this track (resets on every load)
     "settings": {"PeakExtractor": {"MIN_PROMINENCE": 0.2}}}   on top of the pipeline's own defaults

Each run appends one line to runs/log.jsonl and, unless --no-frames, writes its frames to runs/NNN.json (2-8 MB each;
diagnose.py needs them). Runs take about as long as the track at hop 64 (a 60 s track: ~105 s). Run one batch at a time:
every run needs port 9001.
"""
import argparse
import json
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import workspace
from adagio import Engine
from metric import Reference, score_frames, notes_f1


def next_index(runs):
    log = runs / 'log.jsonl'
    if not log.exists():
        return 1
    with open(log) as f:
        return sum(1 for _ in f) + 1


def line(n, name, r, extra=''):
    return (f"#{n:3d} {name:44s} total {r['total']:.3f}  mel {r['melody']:.3f}  harm {r['harmony']:.3f}  "
            f"prec {r['precision']:.3f}  chord {r['chord']:.3f}  key {r['key']:.3f}  F1 {notes_f1(r):.3f}  "
            f"n/f {r['notesPerFrame']:.2f}{extra}")


class Runner:
    """Keeps one engine alive across configs that share engine parameters."""

    def __init__(self, cfg):
        self.cfg = cfg
        self.engine = None
        self.params = None
        self.pipeline = None
        self.log = open(cfg['runs'] / 'engine.log', 'a')

    def start(self, params):
        self.stop()
        self.engine = Engine(self.cfg['exe'], self.cfg['runs'] / 'pipelines.json', params, log=self.log)
        self.params = params
        self.pipeline = None
        self.engine.load(self.cfg['wav'])

    def prepare(self, run):
        params = {**self.cfg['engine'], **run.get('engine', {})}
        if self.engine is None or params != self.params:
            self.start(params)
        else:
            self.engine.reconnect()
        pipeline = run.get('pipeline', self.cfg['pipeline'])
        if pipeline != self.pipeline:
            self.engine.check('switchPipeline', pipeline)
            self.pipeline = pipeline
        self.engine.apply(run.get('settings', {}), run.get('a4'))
        return params, pipeline

    def stop(self):
        if self.engine:
            self.engine.close()
            self.engine = None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('batch')
    ap.add_argument('--work', default='.')
    ap.add_argument('--no-frames', action='store_true')
    a = ap.parse_args()
    cfg = workspace.load(a.work)
    batch = json.load(open(workspace.path(cfg, a.batch)))
    if isinstance(batch, dict):
        batch = [batch]
    ref = Reference(cfg['grid'], cfg['truth'])
    runner = Runner(cfg)
    try:
        for run in batch:
            frames = None
            for attempt in range(3):
                try:
                    params, pipeline = runner.prepare(run)
                    t0 = time.time()
                    frames = runner.engine.step_all()
                    break
                except Exception as ex:   # a dropped socket: restart the engine and run the same config again
                    print('  run failed, retrying:', repr(ex), flush=True)
                    runner.stop()
            if frames is None:
                raise RuntimeError(f'three failed attempts at {run.get("name")}')
            elapsed = time.time() - t0
            n = next_index(cfg['runs'])
            if not a.no_frames:
                json.dump(frames, open(cfg['runs'] / f'{n:03d}.json', 'w'))
            r = score_frames(frames, ref)
            entry = {'iteration': n, 'name': run.get('name', ''), 'pipeline': pipeline, 'engine': params,
                     'a4': run.get('a4'), 'settings': run.get('settings', {}), 'seconds': round(elapsed, 1),
                     **{k: round(v, 4) for k, v in r.items()}}
            with open(cfg['runs'] / 'log.jsonl', 'a') as f:
                f.write(json.dumps(entry) + '\n')
            print(line(n, entry['name'], r, f'  ({elapsed:.0f}s)'), flush=True)
    finally:
        runner.stop()


if __name__ == '__main__':
    main()
