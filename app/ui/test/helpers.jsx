import { act, render } from '@testing-library/react';
import { Provider } from 'react-redux';
import { configureStore } from '@reduxjs/toolkit';
import { WebSocketContext } from '../src/engine-client/WebSocketContext';
import { CONNECTION_STATE } from '../src/engine-client/WebSocketEngine';
import appReducer, { setConnectionState } from '../src/store/appSlice';
import playbackReducer, { setTransport } from '../src/store/playbackSlice';
import analysisReducer from '../src/store/analysisSlice';
import settingsReducer from '../src/store/settingsSlice';
import pipelineReducer from '../src/store/pipelineSlice';

// The same reducers as store/Store.jsx, in a fresh store per test.
export const makeStore = () => configureStore({
    reducer: {
        app: appReducer,
        playback: playbackReducer,
        analysis: analysisReducer,
        settings: settingsReducer,
        pipeline: pipelineReducer,
    },
});

// Stands in for WebSocketEngine: the same listener API, and request() answered by a
// handler per command (default { ok: true, value: null }). Every request is recorded.
export class FakeEngine {
    constructor(handlers = {}, state = CONNECTION_STATE.CONNECTED) {
        this.handlers = handlers;
        this.state = state;
        this.calls = [];
        this.listeners = new Set();
        this.frameListeners = new Set();
        this.stateListeners = new Set();
    }

    request(cmd, args) {
        this.calls.push({ cmd, args });
        const handler = this.handlers[cmd];
        return Promise.resolve(handler ? handler(args, this) : { ok: true, value: null });
    }

    sent(cmd) {
        return this.calls.filter(call => call.cmd === cmd);
    }

    emit(msg) {
        this.listeners.forEach(listener => listener(msg));
    }

    frame(frame) {
        this.frameListeners.forEach(listener => listener(frame));
    }

    setState(state) {
        this.state = state;
        this.stateListeners.forEach(listener => listener(state));
    }

    addListener(callback) { this.listeners.add(callback); }
    removeListener(callback) { this.listeners.delete(callback); }
    addFrameListener(callback) { this.frameListeners.add(callback); }
    removeFrameListener(callback) { this.frameListeners.delete(callback); }
    addStateListener(callback) { this.stateListeners.add(callback); }
    removeStateListener(callback) { this.stateListeners.delete(callback); }
}

export const renderWithEngine = (ui, { engine = new FakeEngine(), store = makeStore() } = {}) => {
    const result = render(
        <WebSocketContext.Provider value={engine}>
            <Provider store={store}>{ui}</Provider>
        </WebSocketContext.Provider>,
    );
    return { ...result, engine, store };
};

export const TRACK = {
    path: 'C:\\music\\Lady.mp3',
    sampleRate: 44100,
    channels: 2,
    duration: 312,
    analysisSampleRate: 8000,
};

// Puts the store where a transport event for an open track would.
export const openTrack = (store, { state = 'paused', speed = 1, volume = 0.2, track = TRACK } = {}) => {
    act(() => {
        store.dispatch(setConnectionState(CONNECTION_STATE.CONNECTED));
        store.dispatch(setTransport({ state, speed, volume, track }));
    });
};

// Lets promise chains started by an event (a command and its .then) run to the end.
export const flush = () => act(async () => {
    for (let i = 0; i < 5; i++)
        await Promise.resolve();
});

// Runs pending animation frames: the sidebar and keyboard paint in one.
export const nextFrame = () => act(() => new Promise(resolve => requestAnimationFrame(() => resolve())));

// One analysis event's payload, shaped as AnalysisPipeline::GetResultJson sends it.
export const analysisEvent = ({
    key = { name: 'Gb Major', tonic: 'Gb', tonicClass: 6, mode: 'Major', scaleClasses: [6, 8, 10, 11, 1, 3, 5] },
    keyHistogram = [{ frequency: 369.99, score: 4 }, { frequency: 466.16, score: 2 }],
    chords = [{ name: 'Bbm9', probability: 0.404 }, { name: 'Db6', probability: 0.298 }],
    notes = [{ name: 'Bb', frequency: 233.08, errorCents: 2, midi: 58 }],
    sampleRate = 8000,
} = {}) => ({
    type: 'analysis',
    value: {
        notes,
        sampleRate,
        executionTimeMs: 1.5,
        keyHistogram,
        chordHistogram: keyHistogram,
        detectedKey: key,
        predictedChords: chords,
    },
});
