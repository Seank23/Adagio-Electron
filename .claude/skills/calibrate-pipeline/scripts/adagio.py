"""Drive the real Adagio engine over its socket: load a file, apply settings, step through it frame by frame and collect
every analysis event. Each stepFrame moves the playhead one hop and analyses that frame, weighted by one hop, exactly as
playback would, so a run is the engine's own output with no audio device in the way."""
import json
import os
import struct
import subprocess
import sys
import threading
import time

import websocket

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import workspace

PORT, ORIGIN = workspace.protocol()
SPECTRUM_KINDS = (1, 3)   # protocol.json binaryFrames: spectrum, logSpectrum


def frame_of(value, t):
    """The parts of an analysis event the metric reads."""
    return {
        't': t,
        'notes': [(n['midi'], n['score'], n['magnitude'], n['errorCents']) for n in value['notes'] or []],
        'chords': [(c['name'], c['probability']) for c in (value['predictedChords'] or [])[:3]],
        'key': value['detectedKey']['name'],
        'tuning': value['avgTuningError'],
    }


class Engine:
    def __init__(self, exe, pipelines, engine_params, log=None):
        if workspace.port_in_use(PORT):
            raise RuntimeError(f'port {PORT} is in use: close Adagio (or another calibration run) first')
        p = engine_params
        args = [str(exe), f'--pipelines={pipelines}', f"--sample-rate={p['sampleRate']}",
                f"--frame-length={p['frameLength']}", f"--hop-size={p['hopSize']}"]
        self.proc = subprocess.Popen(args, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        self.lines = []
        self.ready = threading.Event()
        threading.Thread(target=self._read_stdout, args=(log,), daemon=True).start()
        if not self.ready.wait(20):
            self.proc.kill()
            raise RuntimeError('engine did not start: ' + '\n'.join(self.lines))
        self.ws = self._connect()
        self.next_id = 1
        self.events = []

    def _connect(self):
        for _ in range(50):
            try:
                return websocket.create_connection(f'ws://127.0.0.1:{PORT}', origin=ORIGIN, timeout=60)
            except OSError:
                time.sleep(0.1)
        raise RuntimeError('could not connect to the engine')

    def _read_stdout(self, log):
        for raw in self.proc.stdout:
            line = raw.decode(errors='replace').rstrip()
            self.lines.append(line)
            if log:
                log.write(line + '\n')
                log.flush()
            if 'Adagio engine ready' in line:
                self.ready.set()

    def send(self, cmd, args=None):
        rid = self.next_id
        self.next_id += 1
        msg = {'id': rid, 'cmd': cmd}
        if args is not None:
            msg['args'] = args
        self.ws.send(json.dumps(msg))
        return rid

    def receive(self):
        """One message: ('binary', header dict) or ('json', dict)."""
        opcode, data = self.ws.recv_data()
        if opcode == websocket.ABNF.OPCODE_CLOSE:
            # The engine drops a client that has not answered its pings for about a second (close code 1011,
            # "Ping timeout"). Read as text, this looks like a malformed message; it is not an engine bug.
            code = struct.unpack('>H', data[:2])[0] if len(data) >= 2 else None
            raise ConnectionError(f'engine closed the socket: {code} {bytes(data[2:]).decode(errors="replace")}')
        if opcode == websocket.ABNF.OPCODE_BINARY:
            kind, version, elem, gen, ts, res, mx, count, start = struct.unpack_from('<BBHIdffIf', data, 0)
            return 'binary', {'kind': kind, 'timestamp': ts, 'count': count}
        return 'json', json.loads(data)

    def request(self, cmd, args=None):
        rid = self.send(cmd, args)
        while True:
            kind, msg = self.receive()
            if kind == 'json' and msg.get('type') == 'reply' and str(msg.get('id')) == str(rid):
                return msg
            if kind == 'json':
                self.events.append(msg)

    def check(self, cmd, args=None):
        r = self.request(cmd, args)
        if not r['ok']:
            raise RuntimeError(f'{cmd} {args!r}: {r.get("error")}')
        return r.get('value')

    def reconnect(self):
        """A fresh socket. Call before every run: scoring the last run left this one unread long enough to be dropped."""
        try:
            self.ws.close()
        except Exception:
            pass
        self.ws = self._connect()
        self.events = []

    def close(self):
        try:
            self.ws.close()
        except Exception:
            pass
        try:
            self.proc.stdin.close()   # the engine shuts down on stdin EOF
            self.proc.wait(10)
        except Exception:
            self.proc.kill()

    # ------------------------------------------------------------------ high level
    def load(self, path):
        self.events.clear()
        return self.check('load', str(path))

    def apply(self, settings, a4=None):
        """settings: {stage: {key: value}} on top of the active pipeline's defaults. A4 is per track and
        restorePipelineDefaults resets it too, so it goes last."""
        self.check('restorePipelineDefaults')
        for stage, kv in settings.items():
            for key, value in kv.items():
                self.check('setAnalysisSetting', {'stage': stage, 'key': key, 'value': value})
        if a4 is not None:
            self.check('setAnalysisSetting', {'stage': 'NoteDetector', 'key': 'A440_TUNING', 'value': a4})

    def step_all(self, max_steps=1_000_000, batch=64):
        """Rewind, clear the trackers, then step to the end. Returns one frame dict per analysis event.

        Steps go out in batches: 64 keeps the socket busy without the engine falling behind (128 with dense output
        dropped connections; 16 spent most of its time on round trips)."""
        self.check('seek', 0)
        self.request('resetAnalysis')
        self.events.clear()
        frames = []
        pending_ts = None
        done = False
        steps = 0
        while not done and steps < max_steps:
            ids = [self.send('stepFrame') for _ in range(batch)]
            steps += batch
            outstanding = set(str(i) for i in ids)
            while outstanding:
                kind, msg = self.receive()
                if kind == 'binary':
                    if msg['kind'] in SPECTRUM_KINDS:
                        pending_ts = msg['timestamp']
                    continue
                t = msg.get('type')
                if t == 'reply':
                    rid = str(msg.get('id'))
                    if rid in outstanding:
                        outstanding.discard(rid)
                        if not msg['ok']:
                            done = True   # stepFrame is refused at the end of the file
                elif t == 'analysis':
                    frames.append(frame_of(msg['value'], pending_ts))
        return frames
