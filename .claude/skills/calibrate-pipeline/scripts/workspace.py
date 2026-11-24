"""The calibration workspace: one directory per track, holding

    calibration.json   what to run (below)
    truth.json         the transcription (see references/transcription.md)
    grid.json          each recorded beat's time and score position (written by grid.py)
    runs/              log.jsonl, one NNN.json of frames per run, the engine's scratch pipelines.json

calibration.json:
    {
      "wav": "Track.wav",                                              absolute, or relative (below)
      "pipeline": "constantQ",                                         optional, default constantQ
      "engine": {"sampleRate": 8000, "frameLength": 8192, "hopSize": 64},  optional, these are the defaults
      "exe": ".../engine/build/Release/AdagioEngine.exe"               optional, default the repo's Release build
    }

A relative path is resolved against the workspace, or the repo root if it is only there: a committed example names
its recording (kept out of git) by repo-relative path, so it works from any clone and any copy of the workspace.
Every script takes --work DIR (default: the current directory).
"""
import json
import socket
import sys
from pathlib import Path

if hasattr(sys.stdout, 'reconfigure'):
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')   # note names, the cents sign, flats on a cp1252 console

REPO = Path(__file__).resolve().parents[4]
DEFAULT_EXE = REPO / 'engine' / 'build' / 'Release' / 'AdagioEngine.exe'
DEFAULT_ENGINE = {'sampleRate': 8000, 'frameLength': 8192, 'hopSize': 64}


def load(work='.'):
    work = Path(work).resolve()
    cfg = json.loads((work / 'calibration.json').read_text(encoding='utf-8'))
    cfg['work'] = work
    cfg['pipeline'] = cfg.get('pipeline', 'constantQ')
    cfg['engine'] = {**DEFAULT_ENGINE, **cfg.get('engine', {})}
    for key, default in (('exe', DEFAULT_EXE), ('wav', None), ('truth', 'truth.json'), ('grid', 'grid.json')):
        value = cfg.get(key, default)
        if value is None:
            raise KeyError(f'calibration.json needs "{key}"')
        cfg[key] = _resolve(work, value)
    cfg['runs'] = work / 'runs'
    cfg['runs'].mkdir(exist_ok=True)
    return cfg


def _resolve(work, value):
    path = Path(value)
    if path.is_absolute():
        return path
    if not (work / path).exists() and (REPO / path).exists():
        return REPO / path
    return work / path


def path(cfg, p):
    """A file argument: as given if it exists, else relative to the workspace (the shell's cwd resets between calls)."""
    p = Path(p)
    return p if p.is_absolute() or p.exists() else cfg['work'] / p


def protocol():
    """(port, origin) from the repo's protocol.json, so a hand-started engine accepts us."""
    try:
        p = json.loads((REPO / 'protocol' / 'protocol.json').read_text(encoding='utf-8'))
        transport = next(v for v in p.values() if isinstance(v, dict) and 'port' in v)
        origin = next(o for o in transport['allowedOrigins'] if o.startswith('http'))
        return transport['port'], origin
    except Exception:
        return 9001, 'http://localhost:5173'


def port_in_use(port):
    try:
        with socket.create_connection(('127.0.0.1', port), timeout=0.5):
            return True
    except OSError:
        return False
