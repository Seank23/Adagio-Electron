"""List a pipeline's stages and settings as the engine reports them: key, value in force (the pipeline's default on a
scratch pipelines.json), range or options, unit. Start every sweep plan from this, not from memory: pipeline defaults
can differ from the stage headers' TSettings.

usage: python schema.py [--work DIR] [--pipeline ID] [--json]
"""
import argparse
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import workspace
from adagio import Engine


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--work', default='.')
    ap.add_argument('--pipeline')
    ap.add_argument('--json', action='store_true')
    a = ap.parse_args()
    cfg = workspace.load(a.work)
    e = Engine(cfg['exe'], cfg['runs'] / 'pipelines.json', cfg['engine'])
    try:
        e.check('switchPipeline', a.pipeline or cfg['pipeline'])
        e.check('restorePipelineDefaults')
        schema = e.check('getAnalysisSchema')
    finally:
        e.close()
    if a.json:
        print(json.dumps(schema, indent=1))
        return
    print('pipelines:', ', '.join(p['id'] for p in schema.get('pipelines', [])), '| active:', schema.get('activePipeline'))
    fmt = lambda v: f'{v:.6g}' if isinstance(v, float) else str(v)   # float defaults arrive widened (0.30000001...)
    for i, stage in enumerate(schema['stages'], 1):
        print(f"\n{i}. {stage['name']}")
        for s in stage['settings']:
            value = s.get('value', s.get('default'))
            rng = f"options {s['options']}" if 'options' in s else f"{fmt(s.get('min', ''))}..{fmt(s.get('max', ''))}"
            note = '' if s.get('persistent', True) else '  (per track: never a default)'
            changed = '' if fmt(value) == fmt(s.get('default')) else f"  default {fmt(s.get('default'))}"
            print(f"   {s['key']:24s} {fmt(value):>12s} {s.get('unit', ''):4s} {rng}{changed}{note}")


if __name__ == '__main__':
    main()
