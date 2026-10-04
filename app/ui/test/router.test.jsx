// EngineEventRouter is the only place engine signals become dispatches (§5). These run
// it against a scripted engine and check each signal's effect on the store.
import { describe, expect, it } from 'vitest';
import { act } from '@testing-library/react';
import EngineEventRouter from '../src/router/EngineEventRouter';
import TransportBar from '../src/components/TransportBar';
import { CONNECTION_STATE } from '../src/engine-client/WebSocketEngine';
import { getLatestSpectrum, getWaveform } from '../src/engine-client/FrameStore';
import { BINARY_FRAME } from '../src/utils/protocol';
import { toggleRepeat } from '../src/store/settingsSlice';
import { FakeEngine, TRACK, analysisEvent, flush, renderWithEngine } from './helpers';

const statusWith = value => ({ status: () => ({ ok: true, value }) });

const waveformFrame = resolution => ({ kind: BINARY_FRAME.KIND.WAVEFORM, resolution, data: new Float32Array(4) });
const spectrumFrame = generation => ({ kind: BINARY_FRAME.KIND.SPECTRUM, seekGeneration: generation, count: 4, data: new Uint16Array(4) });

describe('EngineEventRouter on connect', () => {
    it('asks for the status and the analysis schema, and shows the engine\'s transport', async () => {
        const engine = new FakeEngine({
            ...statusWith({ state: 'paused', track: TRACK, speed: 0.75, volume: 0.55 }),
            getAnalysisSchema: () => ({ ok: true, value: { stages: [{ name: 'NoteDetector', settings: [] }] } }),
        });
        const { store } = renderWithEngine(<EngineEventRouter />, { engine });
        await flush();
        expect(engine.sent('status')).toHaveLength(1);
        expect(engine.sent('getAnalysisSchema')).toHaveLength(1);
        const { playback, pipeline, app } = store.getState();
        expect(app.connectionState).toBe(CONNECTION_STATE.CONNECTED);
        expect(playback.track.path).toBe(TRACK.path);
        expect([playback.speed, playback.volume]).toEqual([0.75, 0.55]);
        expect(pipeline.stages).toHaveLength(1);
    });

    it('asks for the waveform with a track open and publishes the frames on the reply', async () => {
        const engine = new FakeEngine({
            ...statusWith({ state: 'playing', track: TRACK }),
            getWaveform: (_, self) => {
                // The frames arrive ahead of the reply, as on the wire.
                [256, 512].forEach(resolution => self.frame(waveformFrame(resolution)));
                return { ok: true, value: 2 };
            },
        });
        renderWithEngine(<EngineEventRouter />, { engine });
        await flush();
        expect(engine.sent('getWaveform')).toHaveLength(1);
        expect(getWaveform().map(set => set.resolution)).toEqual([256, 512]);
    });

    it('does not ask for a waveform with no track', async () => {
        const engine = new FakeEngine(statusWith({ state: 'empty', track: null }));
        renderWithEngine(<EngineEventRouter />, { engine });
        await flush();
        expect(engine.sent('getWaveform')).toEqual([]);
    });

    it('sends no volume on connect or reconnect: a reload keeps the engine\'s', async () => {
        const engine = new FakeEngine(statusWith({ state: 'playing', track: TRACK, volume: 0.55 }), CONNECTION_STATE.CONNECTING);
        const { store } = renderWithEngine(<><EngineEventRouter /><TransportBar /></>, { engine });
        act(() => engine.setState(CONNECTION_STATE.CONNECTED));
        await flush();
        act(() => engine.setState(CONNECTION_STATE.DISCONNECTED));
        act(() => engine.setState(CONNECTION_STATE.CONNECTED));
        await flush();
        expect(engine.sent('status')).toHaveLength(2);
        expect(engine.sent('setVolume')).toEqual([]);
        expect(store.getState().playback.volume).toBe(0.55);
    });
});

describe('EngineEventRouter events', () => {
    const setup = handlers => {
        const engine = new FakeEngine(handlers);
        const rendered = renderWithEngine(<EngineEventRouter />, { engine });
        return { ...rendered, engine, emit: msg => act(() => engine.emit(msg)) };
    };

    it('applies transport and position events', async () => {
        const { store, emit } = setup();
        await flush();
        emit({ type: 'transport', value: { state: 'playing', track: TRACK, speed: 0.5, volume: 0.3 } });
        emit({ type: 'position', value: 84.38 });
        const { playback } = store.getState();
        expect([playback.state, playback.speed, playback.currentTime]).toEqual(['playing', 0.5, 84.38]);
    });

    it('clears the spectrum and the analysis on analysisReset, and on nothing else', async () => {
        const { store, emit, engine } = setup();
        await flush();
        emit(analysisEvent());
        act(() => engine.frame(spectrumFrame(1)));
        expect(getLatestSpectrum()).not.toBeNull();

        emit({ type: 'transport', value: { state: 'paused', track: TRACK } });
        emit({ type: 'position', value: 12 });
        expect(store.getState().analysis.detectedKey.name).toBe('Gb Major');

        emit({ type: 'analysisReset' });
        expect(getLatestSpectrum()).toBeNull();
        const { analysis } = store.getState();
        expect(analysis.detectedKey).toBeNull();
        expect(analysis.predictedChords).toEqual([]);
        expect(analysis.spectrumSR).toBe(8000);
    });

    it('drops a spectrum frame older than the one shown', async () => {
        const { engine } = setup();
        await flush();
        act(() => engine.frame({ ...spectrumFrame(4), id: 'after seek' }));
        act(() => engine.frame({ ...spectrumFrame(3), id: 'before seek' }));
        expect(getLatestSpectrum().id).toBe('after seek');
    });

    it('restarts the track on endOfPlay with repeat on, and not with it off', async () => {
        const { store, emit, engine } = setup();
        await flush();
        emit({ type: 'endOfPlay' });
        await flush();
        expect(engine.sent('play')).toEqual([]);
        act(() => store.dispatch(toggleRepeat()));
        emit({ type: 'endOfPlay' });
        await flush();
        expect(engine.sent('play')).toHaveLength(1);
    });

    it('shows a refused repeat in the status bar', async () => {
        const { store, emit } = setup({ play: () => ({ ok: false, error: 'Nope.' }) });
        await flush();
        act(() => store.dispatch(toggleRepeat()));
        emit({ type: 'endOfPlay' });
        await flush();
        expect(store.getState().app.statusMessage).toEqual({ type: 'error', message: 'Nope.' });
    });

    it('publishes the staged waveform and clears the spectrum on fileLoaded, and drops both on fileClosed', async () => {
        const { store, emit, engine } = setup();
        await flush();
        act(() => engine.frame(spectrumFrame(0)));
        act(() => engine.frame(waveformFrame(256)));
        expect(getWaveform()).toBeNull();
        emit({ type: 'fileLoaded', value: { duration: 2 } });
        expect(getWaveform()).toHaveLength(1);
        expect(getLatestSpectrum()).toBeNull();
        expect(store.getState().playback.duration).toBe(2);

        emit({ type: 'fileClosed' });
        expect(getWaveform()).toBeNull();
        expect(store.getState().playback.duration).toBe(0);
    });

    it('turns error and info events into status messages', async () => {
        const { store, emit } = setup();
        await flush();
        emit({ type: 'error', value: 'Could not decode.' });
        expect(store.getState().app.statusMessage).toEqual({ type: 'error', message: 'Could not decode.' });
    });

    it('follows main\'s engine status through window.api', async () => {
        let push;
        window.api = { onEngineStatus: callback => { push = callback; return () => {}; } };
        const { store } = setup();
        act(() => push({ state: 'failed', error: 'AdagioEngine.exe: ENOENT' }));
        expect(store.getState().app.engineStatus).toEqual({ state: 'failed', error: 'AdagioEngine.exe: ENOENT' });
    });
});
