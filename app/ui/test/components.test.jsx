import { Profiler } from 'react';
import { describe, expect, it, vi } from 'vitest';
import { act, fireEvent, screen, within } from '@testing-library/react';
import TopBar from '../src/components/TopBar';
import TransportBar from '../src/components/TransportBar';
import StatusBar from '../src/components/StatusBar';
import EmptyState from '../src/components/EmptyState';
import TuningControl from '../src/components/TuningControl';
import SpectrumPanel from '../src/components/SpectrumPanel';
import Sidebar from '../src/components/sidebar/Sidebar';
import { setAnalysisData } from '../src/store/analysisSlice';
import { setConnectionState, setEngineStatus, setLoadingFile } from '../src/store/appSlice';
import { setCurrentTime, setTransport } from '../src/store/playbackSlice';
import { setSchema } from '../src/store/pipelineSlice';
import { setSidebarTab } from '../src/store/settingsSlice';
import { FakeEngine, analysisEvent, flush, nextFrame, openTrack, renderWithEngine } from './helpers';

const button = name => screen.getByRole('button', { name });

describe('TopBar', () => {
    it('shows the track from the engine: name and meta line', () => {
        const { store } = renderWithEngine(<TopBar />);
        expect(screen.getByText('No file open')).toBeTruthy();
        openTrack(store);
        expect(screen.getByText('Lady.mp3')).toBeTruthy();
        expect(screen.getByText('MP3 · 44.1 kHz · stereo · 5:12')).toBeTruthy();
    });

    it('swaps Play for Pause only when the transport event arrives, never on the click', async () => {
        const { store, engine } = renderWithEngine(<TopBar />);
        openTrack(store, { state: 'paused' });
        fireEvent.click(button('Play'));
        await flush();
        expect(engine.sent('play')).toHaveLength(1);
        expect(button('Play')).toBeTruthy();
        act(() => store.dispatch(setTransport({ state: 'playing' })));
        fireEvent.click(button('Pause'));
        await flush();
        expect(engine.sent('pause')).toHaveLength(1);
    });

    it('disables the transport with no file, and Step while playing', () => {
        const { store } = renderWithEngine(<TopBar />);
        for (const name of ['Play', 'Stop', 'Back to start', 'Step one analysis frame (.)', /Reset analysis/])
            expect(button(name).disabled).toBe(true);
        openTrack(store, { state: 'playing' });
        expect(button('Step one analysis frame (.)').disabled).toBe(true);
        expect(button(/Reset analysis/).disabled).toBe(false);
        openTrack(store, { state: 'ready' });
        expect(button('Step one analysis frame (.)').disabled).toBe(false);
    });

    it('leaves the analysis alone on the Reset button\'s reply: only the event clears it', async () => {
        const { store, engine } = renderWithEngine(<TopBar />);
        openTrack(store);
        act(() => store.dispatch(setAnalysisData(analysisEvent().value)));
        fireEvent.click(button(/Reset analysis/));
        await flush();
        expect(engine.sent('resetAnalysis')).toHaveLength(1);
        expect(store.getState().analysis.detectedKey.name).toBe('Gb Major');
    });

    it('reports a refused command in the status bar', async () => {
        const engine = new FakeEngine({ stop: () => ({ ok: false, error: 'Not now.' }) });
        const { store } = renderWithEngine(<TopBar />, { engine });
        openTrack(store);
        fireEvent.click(button('Stop'));
        await flush();
        expect(store.getState().app.statusMessage).toEqual({ type: 'error', message: 'Not now.' });
    });
});

describe('TransportBar', () => {
    it('highlights a speed preset only when the engine\'s speed matches it', () => {
        const { store } = renderWithEngine(<TransportBar />);
        const pressed = () => ['50', '75', '100'].filter(name => button(name).getAttribute('aria-pressed') === 'true');
        openTrack(store, { speed: 0.5 });
        expect(pressed()).toEqual(['50']);
        openTrack(store, { speed: 0.2 });
        expect(pressed()).toEqual([]);
    });

    it('sends a preset and waits for the engine to move', async () => {
        const { store, engine } = renderWithEngine(<TransportBar />);
        openTrack(store, { speed: 1 });
        fireEvent.click(button('75'));
        await flush();
        expect(engine.sent('setSpeed').map(call => call.args)).toEqual([0.75]);
        expect(button('100').getAttribute('aria-pressed')).toBe('true');
    });

    it('names its sliders and says whether each toggle is on', () => {
        const { store } = renderWithEngine(<TransportBar />);
        openTrack(store, { volume: 0.2 });
        expect(screen.getByRole('slider', { name: 'Volume' }).getAttribute('aria-valuenow')).toBe('20');
        expect(screen.getByRole('slider', { name: 'Speed' })).toBeTruthy();
        expect(button('Repeat').getAttribute('aria-pressed')).toBe('false');
        expect(button('Follow the playhead').getAttribute('aria-pressed')).toBe('true');
        fireEvent.click(button('Repeat'));
        expect(button('Repeat').getAttribute('aria-pressed')).toBe('true');
    });

    it('shows the position and the length', () => {
        const { store } = renderWithEngine(<TransportBar />);
        openTrack(store);
        act(() => store.dispatch(setCurrentTime(84.38)));
        expect(screen.getByText('01:24.380')).toBeTruthy();
        expect(screen.getByText('05:12.000')).toBeTruthy();
    });
});

describe('StatusBar', () => {
    const indicator = () => screen.getByRole('status').textContent;

    it('shows Connecting, Disconnected and connected from the socket', () => {
        const { store } = renderWithEngine(<StatusBar />);
        expect(indicator()).toBe('Connecting');
        act(() => store.dispatch(setConnectionState('disconnected')));
        expect(indicator()).toBe('Disconnected');
        act(() => store.dispatch(setConnectionState('connected')));
        expect(indicator()).toBe('Engine connected');
        expect(screen.getByText('Ready. Open or drop a file to begin')).toBeTruthy();
    });

    it('shows a failed spawn while the socket is still retrying', () => {
        const { store } = renderWithEngine(<StatusBar />);
        act(() => store.dispatch(setEngineStatus({ state: 'failed', error: 'ENOENT' })));
        expect(indicator()).toBe('Engine unavailable');
    });

    it('shows the analysis rate as soon as a track is open', () => {
        const { store } = renderWithEngine(<StatusBar />);
        openTrack(store);
        expect(screen.getByText(/^Analysis \d+ frames\/s/)).toBeTruthy();
    });

    it('clears a message after 3 s, but keeps a load in progress', () => {
        vi.useFakeTimers();
        try {
            const { store } = renderWithEngine(<StatusBar />);
            act(() => store.dispatch({ type: 'app/setStatusMessage', payload: { type: 'error', message: 'Oops' } }));
            act(() => vi.advanceTimersByTime(3100));
            expect(store.getState().app.statusMessage.message).toBe('');
            act(() => store.dispatch({ type: 'app/setStatusMessage', payload: { type: 'loading', message: 'Loading audio...' } }));
            act(() => vi.advanceTimersByTime(10000));
            expect(store.getState().app.statusMessage.message).toBe('Loading audio...');
        } finally {
            vi.useRealTimers();
        }
    });
});

describe('EmptyState', () => {
    it('shows the file being loaded and disables Open meanwhile', () => {
        const { store } = renderWithEngine(<EmptyState isDragging={false} />);
        expect(screen.getByText('Drop an audio file to analyse')).toBeTruthy();
        act(() => store.dispatch(setLoadingFile('C:\\music\\Tone.wav')));
        expect(within(screen.getByRole('status')).getByText('Tone.wav')).toBeTruthy();
        expect(button(/Open file/).disabled).toBe(true);
    });
});

describe('Sidebar', () => {
    const rows = () => Object.fromEntries(
        screen.getAllByText(/^(C|Db|D|Eb|E|F|Gb|G|Ab|A|Bb|B)$/).map(note => [note.textContent, note.parentElement]),
    );
    const barWidth = row => row.querySelector(':scope > div > div').style.width;

    it('shows — and no chord rows before any analysis', () => {
        renderWithEngine(<Sidebar />);
        expect(screen.getByText('—')).toBeTruthy();
        expect(screen.getAllByText(/^(C|Db|D|Eb|E|F|Gb|G|Ab|A|Bb|B)$/)).toHaveLength(12);
    });

    it('marks the tonic by tonicClass and fills the largest bar to 90%', async () => {
        const { store } = renderWithEngine(<Sidebar />);
        act(() => store.dispatch(setAnalysisData(analysisEvent({
            keyHistogram: [{ frequency: 369.99, score: 4 }, { frequency: 466.16, score: 2 }],
        }).value)));
        await nextFrame();
        expect(screen.getByText('Gb Major')).toBeTruthy();
        const byNote = rows();
        expect(byNote.Gb.firstChild.style.color).toBe('var(--accent-secondary-text)');
        expect(byNote.C.firstChild.style.color).toBe('var(--text-muted)');
        expect(barWidth(byNote.Gb)).toBe('90%');
        expect(barWidth(byNote.Bb)).toBe('45%');
        expect(byNote.Gb.lastChild.textContent).toBe('66.7%');
    });

    it('shows chord probabilities, sent as fractions, as percentages', async () => {
        const { store } = renderWithEngine(<Sidebar />);
        act(() => store.dispatch(setAnalysisData(analysisEvent({
            chords: [{ name: 'Bbm9', probability: 0.404 }, { name: 'Db6', probability: 0.202 }],
        }).value)));
        await nextFrame();
        const top = screen.getByText('Bbm9').parentElement;
        expect(top.lastChild.textContent).toBe('40.4%');
        expect(top.querySelector(':scope > div > div').style.width).toBe('90%');
        expect(screen.getByText('Db6').parentElement.lastChild.textContent).toBe('20.2%');
    });

    it('re-renders only when the key or the chord names change, not per event', async () => {
        let commits = 0;
        const { store } = renderWithEngine(<Profiler id="sidebar" onRender={() => commits++}><Sidebar /></Profiler>);
        act(() => store.dispatch(setAnalysisData(analysisEvent().value)));
        await nextFrame();
        const settled = commits;
        for (let i = 1; i <= 20; i++) {
            act(() => store.dispatch(setAnalysisData(analysisEvent({
                keyHistogram: [{ frequency: 369.99, score: 4 + i }, { frequency: 466.16, score: 2 }],
                chords: [{ name: 'Bbm9', probability: 0.4 + i / 100 }, { name: 'Db6', probability: 0.2 }],
            }).value)));
        }
        await nextFrame();
        expect(commits).toBe(settled);
    });

    it('keeps the disabled Settings tab out of reach, and unmounts a hidden tab', () => {
        const { store } = renderWithEngine(<Sidebar />);
        const settings = screen.getByRole('tab', { name: 'Settings' });
        expect(settings.disabled).toBe(true);
        expect(settings.tabIndex).toBe(-1);
        expect(screen.getByRole('tab', { name: 'Analysis' }).tabIndex).toBe(0);
        // Asking for the disabled tab still shows Analysis.
        act(() => store.dispatch(setSidebarTab('settings')));
        expect(screen.getByRole('tab', { name: 'Analysis' }).getAttribute('aria-selected')).toBe('true');
        expect(screen.getAllByRole('tabpanel')).toHaveLength(1);
    });

    it('keeps the arrow keys inside the tab list', () => {
        renderWithEngine(<Sidebar />);
        const outside = vi.fn();
        window.addEventListener('keydown', outside);
        fireEvent.keyDown(screen.getByRole('tab', { name: 'Analysis' }), { key: 'ArrowRight' });
        window.removeEventListener('keydown', outside);
        expect(outside).not.toHaveBeenCalled();
    });
});

describe('render budget', () => {
    it('re-renders neither the top bar nor the sidebar on position updates', () => {
        const commits = { top: 0, side: 0 };
        const { store } = renderWithEngine(
            <>
                <Profiler id="top" onRender={() => commits.top++}><TopBar /></Profiler>
                <Profiler id="side" onRender={() => commits.side++}><Sidebar /></Profiler>
            </>,
        );
        openTrack(store, { state: 'playing' });
        const before = { ...commits };
        for (let i = 0; i < 30; i++)
            act(() => store.dispatch(setCurrentTime(i / 30)));
        expect(commits).toEqual(before);
    });
});

describe('TuningControl', () => {
    const SCHEMA = { stages: [{ name: 'NoteDetector', settings: [{ key: 'A440_TUNING', type: 'float', min: 415, max: 466, value: 440 }] }] };
    const value = () => screen.getByRole('slider', { name: 'A440 reference' });

    const setup = (handlers, state = 'paused') => {
        const engine = new FakeEngine(handlers);
        const rendered = renderWithEngine(<TuningControl />, { engine });
        openTrack(rendered.store, { state });
        act(() => rendered.store.dispatch(setSchema(SCHEMA)));
        return rendered;
    };

    it('is disabled until the schema arrives', () => {
        renderWithEngine(<TuningControl />);
        expect(value().getAttribute('aria-disabled')).toBe('true');
    });

    it('shows the engine\'s value only after an ok reply, then analyses the paused frame', async () => {
        const { engine } = setup();
        fireEvent.click(button('Raise the A4 reference'));
        await flush();
        expect(engine.sent('setAnalysisSetting').map(call => call.args)).toEqual([{ stage: 'NoteDetector', key: 'A440_TUNING', value: 440.5 }]);
        expect(value().getAttribute('aria-valuenow')).toBe('440.5');
        expect(engine.sent('analyseFrame')).toHaveLength(1);
    });

    it('does not ask for a frame while playing: the next one picks the setting up', async () => {
        const { engine } = setup({}, 'playing');
        fireEvent.keyDown(value(), { key: 'ArrowUp' });
        await flush();
        expect(engine.sent('setAnalysisSetting')).toHaveLength(1);
        expect(engine.sent('analyseFrame')).toEqual([]);
    });

    it('snaps back when the engine refuses, and says why', async () => {
        const { store } = setup({ setAnalysisSetting: () => ({ ok: false, error: "Setting 'A440_TUNING' is between 415 and 466." }) });
        fireEvent.keyDown(value(), { key: 'ArrowDown', shiftKey: true });
        await flush();
        expect(value().getAttribute('aria-valuenow')).toBe('440');
        expect(store.getState().app.statusMessage.type).toBe('error');
    });

    it('stays inside the range and returns to 440 on a double-click', async () => {
        const { engine, store } = setup();
        act(() => store.dispatch({ type: 'pipeline/setSettingValue', payload: { stage: 'NoteDetector', key: 'A440_TUNING', value: 466 } }));
        expect(button('Raise the A4 reference').disabled).toBe(true);
        fireEvent.keyDown(value(), { key: 'ArrowUp' });
        fireEvent.doubleClick(value());
        await flush();
        expect(engine.sent('setAnalysisSetting').map(call => call.args.value)).toEqual([440]);
    });
});

describe('TransportBar with no file', () => {
    it('leaves Volume usable and makes the rest inert', () => {
        renderWithEngine(<TransportBar />);
        const volume = screen.getByText('Volume').parentElement;
        expect(volume.hasAttribute('inert')).toBe(false);
        expect(screen.getByText('Speed').parentElement.hasAttribute('inert')).toBe(true);
    });
});

describe('SpectrumPanel', () => {
    const ANALYSIS = { sampleRate: 8000, frameLength: 4096, hopSize: 128, frameSmoothing: 4 };

    it('shows the smoothing in force next to the frequency range, and follows the engine', () => {
        const { store } = renderWithEngine(<SpectrumPanel />);
        openTrack(store);
        act(() => store.dispatch(setTransport({ analysis: ANALYSIS })));
        expect(screen.getByText('50 Hz – 4 kHz')).toBeTruthy();
        expect(screen.getByText('4-frame smoothing · 64 ms')).toBeTruthy();
        act(() => store.dispatch(setTransport({ analysis: { ...ANALYSIS, hopSize: 32, frameSmoothing: 10 } })));
        expect(screen.getByText('10-frame smoothing · 40 ms')).toBeTruthy();
        act(() => store.dispatch(setTransport({ analysis: { ...ANALYSIS, frameSmoothing: 1 } })));
        expect(screen.getByText('no smoothing')).toBeTruthy();
    });

    it('shows neither with no file open', () => {
        const { store } = renderWithEngine(<SpectrumPanel />);
        act(() => store.dispatch(setTransport({ state: 'empty', track: null, analysis: ANALYSIS })));
        expect(screen.queryByText(/smoothing/)).toBeNull();
    });
});
