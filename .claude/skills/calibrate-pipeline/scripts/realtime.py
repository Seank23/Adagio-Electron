"""Fidelity check: play the track in real time (volume 0) with one config and score the live analysis events, which the
engine sends every 32 ms of wall time, exactly as the app receives them. Expect it within ~0.01 of the stepped run;
a bigger gap means the stepped harness is not measuring what users see.

usage: python realtime.py config.json [--work DIR]      (one config, as in a batch; "best" works as in sweep.py)
"""
import argparse
import json
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import workspace
from adagio import Engine, SPECTRUM_KINDS, frame_of
from iterate import next_index, line
from metric import Reference, score_frames
from sweep import base_config


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('config')
    ap.add_argument('--work', default='.')
    a = ap.parse_args()
    cfg = workspace.load(a.work)
    run, label = base_config(a.config, cfg)
    params = {**cfg['engine'], **run.get('engine', {})}
    pipeline = run.get('pipeline', cfg['pipeline'])
    ref = Reference(cfg['grid'], cfg['truth'])
    e = Engine(cfg['exe'], cfg['runs'] / 'pipelines.json', params, log=open(cfg['runs'] / 'engine.log', 'a'))
    try:
        e.check('switchPipeline', pipeline)
        e.load(cfg['wav'])
        e.apply(run.get('settings', {}), run.get('a4'))
        e.check('setVolume', 0.0)
        e.check('seek', 0)
        e.request('resetAnalysis')
        e.events.clear()
        frames = []
        last_ts = None
        e.check('play')
        t0 = time.time()
        while time.time() - t0 < 3600:
            kind, msg = e.receive()
            if kind == 'binary':
                if msg['kind'] in SPECTRUM_KINDS:
                    last_ts = msg['timestamp']
                continue
            t = msg.get('type')
            if t == 'analysis' and last_ts is not None:
                frames.append(frame_of(msg['value'], last_ts))
            elif t == 'endOfPlay':
                break
        status = e.check('status')
    finally:
        e.close()
    n = next_index(cfg['runs'])
    json.dump(frames, open(cfg['runs'] / f'{n:03d}.json', 'w'))
    r = score_frames(frames, ref)
    name = f"{run.get('name', label)} [real-time playback]"
    entry = {'iteration': n, 'name': name, 'pipeline': pipeline, 'engine': params, 'a4': run.get('a4'),
             'settings': run.get('settings', {}), 'seconds': round(time.time() - t0, 1),
             'underruns': status.get('underrunCount'), **{k: round(v, 4) for k, v in r.items()}}
    with open(cfg['runs'] / 'log.jsonl', 'a') as f:
        f.write(json.dumps(entry) + '\n')
    print(line(n, name, r, f"  events {len(frames)}  underruns {status.get('underrunCount')}"))


if __name__ == '__main__':
    main()
