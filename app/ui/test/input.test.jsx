import { beforeEach, describe, expect, it, vi } from 'vitest';
import { useEffect } from 'react';
import { act, fireEvent } from '@testing-library/react';
import { readFileSync } from 'node:fs';
import { join } from 'node:path';
import { useKeyboardShortcuts } from '../src/hooks/useKeyboardShortcuts';
import { useFileDrop } from '../src/hooks/useFileDrop';
import { useFrameStep } from '../src/hooks/useStepRepeat';
import { FakeEngine, TRACK, flush, openTrack, renderWithEngine } from './helpers';

const Shortcuts = () => {
    useKeyboardShortcuts();
    return <div><button type="button">Play</button><div role="slider" tabIndex={0} aria-valuenow={0}>slider</div><input aria-label="text" /></div>;
};

// fireEvent returns false when the handler called preventDefault.
const press = (key, init = {}, target = window) => {
    let notPrevented;
    act(() => {
        notPrevented = fireEvent.keyDown(target, { key, ...init });
    });
    return !notPrevented;
};

describe('keyboard shortcuts', () => {
    const setup = (transport = {}, handlers = {}) => {
        const rendered = renderWithEngine(<Shortcuts />, { engine: new FakeEngine(handlers) });
        openTrack(rendered.store, transport);
        return rendered;
    };

    it('toggles play with Space and cancels both halves, so a focused button does not toggle too', async () => {
        const { engine, getByText } = setup({ state: 'paused' });
        const button = getByText('Play');
        button.focus();
        expect(press(' ', {}, button)).toBe(true);
        let keyUpNotPrevented;
        act(() => { keyUpNotPrevented = fireEvent.keyUp(button, { key: ' ' }); });
        expect(keyUpNotPrevented).toBe(false);
        press(' ', { repeat: true }, button);
        await flush();
        expect(engine.sent('play')).toHaveLength(1);
        expect(engine.sent('pause')).toEqual([]);
    });

    it('pauses with Space while playing', async () => {
        const { engine } = setup({ state: 'playing' });
        press(' ');
        await flush();
        expect(engine.sent('pause')).toHaveLength(1);
    });

    it('leaves the arrow keys and Space to a focused slider or a text field', async () => {
        const { engine, getByRole, getByLabelText } = setup();
        expect(press('ArrowRight', {}, getByRole('slider'))).toBe(false);
        expect(press(' ', {}, getByLabelText('text'))).toBe(false);
        await flush();
        expect(engine.calls).toEqual([]);
    });

    it('seeks 5 s with the arrows, clamped to the track', async () => {
        const { engine, store } = setup();
        act(() => store.dispatch({ type: 'playback/setCurrentTime', payload: 310 }));
        press('ArrowRight');
        act(() => store.dispatch({ type: 'playback/setCurrentTime', payload: 2 }));
        press('ArrowLeft');
        await flush();
        expect(engine.sent('seek').map(call => call.args)).toEqual([TRACK.duration, 0]);
    });

    it('ignores playback keys with no file, but Ctrl+O still opens the dialog', async () => {
        const selectAudioFile = vi.fn().mockResolvedValue(null);
        window.api = { selectAudioFile };
        const engine = new FakeEngine();
        renderWithEngine(<Shortcuts />, { engine });
        press(' ');
        press('ArrowRight');
        press('.');
        expect(press('o', { ctrlKey: true })).toBe(true);
        press('o', { ctrlKey: true, repeat: true });
        await flush();
        expect(engine.calls).toEqual([]);
        expect(selectAudioFile).toHaveBeenCalledTimes(1);
    });

    it('steps with . while paused and not while playing', async () => {
        const { engine, store } = setup({ state: 'playing' });
        press('.');
        await flush();
        expect(engine.sent('stepFrame')).toEqual([]);
        openTrack(store, { state: 'paused' });
        press('.');
        await flush();
        expect(engine.sent('stepFrame')).toHaveLength(1);
    });
});

describe('the step gate', () => {
    // The gate is module state shared by the button and the key, with a 33 ms minimum
    // between sends; let the last test's step age out.
    beforeEach(() => new Promise(resolve => setTimeout(resolve, 40)));

    // The hook's callback, handed out from an effect rather than written during render.
    const handle = {};
    const step = () => handle.step();
    const Stepper = () => {
        const frameStep = useFrameStep();
        useEffect(() => {
            handle.step = frameStep;
        });
        return null;
    };

    it('keeps one stepFrame in flight however often it is asked', async () => {
        let answer;
        const engine = new FakeEngine({ stepFrame: () => new Promise(resolve => { answer = resolve; }) });
        const { store } = renderWithEngine(<Stepper />, { engine });
        openTrack(store, { state: 'paused' });
        const first = step();
        const extra = await Promise.all([step(), step(), step()]);
        expect(extra).toEqual([null, null, null]);
        expect(engine.sent('stepFrame')).toHaveLength(1);
        answer({ ok: true });
        await first;
    });

    it('reports a refused step once, in the status bar', async () => {
        const engine = new FakeEngine({ stepFrame: () => ({ ok: false, error: 'At the end of the track.' }) });
        const { store } = renderWithEngine(<Stepper />, { engine });
        openTrack(store, { state: 'paused' });
        await act(() => step());
        expect(store.getState().app.statusMessage).toEqual({ type: 'error', message: 'At the end of the track.' });
    });

    it('sends nothing while playing or with no file', async () => {
        const engine = new FakeEngine();
        const { store } = renderWithEngine(<Stepper />, { engine });
        expect(await step()).toBeNull();
        openTrack(store, { state: 'playing' });
        expect(await step()).toBeNull();
        expect(engine.sent('stepFrame')).toEqual([]);
    });
});

describe('drag and drop', () => {
    const DropZone = ({ onFile }) => {
        const dragging = useFileDrop({ onFile });
        return <div data-dragging={dragging}>zone</div>;
    };

    const fileTransfer = file => ({ types: ['Files'], files: file ? [file] : [], dropEffect: 'none' });

    const setup = () => {
        const onFile = vi.fn();
        const rendered = renderWithEngine(<DropZone onFile={onFile} />);
        return { ...rendered, onFile };
    };

    it('cancels every dragover and drop on the window, so a stray drop cannot navigate it', () => {
        setup();
        let notPrevented;
        act(() => { notPrevented = fireEvent.dragOver(window, { dataTransfer: { types: ['text/plain'] } }); });
        expect(notPrevented).toBe(false);
        act(() => { notPrevented = fireEvent.drop(window, { dataTransfer: { types: ['text/plain'], files: [] } }); });
        expect(notPrevented).toBe(false);
    });

    it('shows the drag state only for files', () => {
        const { getByText } = setup();
        act(() => { fireEvent.dragEnter(window, { dataTransfer: fileTransfer() }); });
        expect(getByText('zone').dataset.dragging).toBe('true');
        // Leaving the window has no related target; jsdom's drag events carry none at all.
        const leave = new Event('dragleave');
        Object.defineProperty(leave, 'relatedTarget', { value: null });
        act(() => { window.dispatchEvent(leave); });
        expect(getByText('zone').dataset.dragging).toBe('false');
    });

    it('loads a supported file by the path preload gives it', () => {
        window.api = { getPathForFile: () => 'C:\\music\\Tone.WAV' };
        const { onFile } = setup();
        act(() => { fireEvent.drop(window, { dataTransfer: fileTransfer(new File([''], 'Tone.WAV')) }); });
        expect(onFile).toHaveBeenCalledWith('C:\\music\\Tone.WAV');
    });

    it('refuses an unsupported file, and a file with no path, with a status message', () => {
        const paths = ['C:\\notes.txt', ''];
        window.api = { getPathForFile: () => paths.shift() };
        const { onFile, store } = setup();
        for (let i = 0; i < 2; i++) {
            act(() => { fireEvent.drop(window, { dataTransfer: fileTransfer(new File([''], 'x')) }); });
            expect(store.getState().app.statusMessage).toEqual({ type: 'error', message: 'Adagio opens WAV, MP3 and FLAC files.' });
        }
        expect(onFile).not.toHaveBeenCalled();
    });

    it('says a drop needs the desktop app outside Electron', () => {
        const { onFile, store } = setup();
        act(() => { fireEvent.drop(window, { dataTransfer: fileTransfer(new File([''], 'Tone.wav')) }); });
        expect(store.getState().app.statusMessage.message).toBe('Opening a file needs the desktop app.');
        expect(onFile).not.toHaveBeenCalled();
    });
});

describe('preload', () => {
    // preload.js is CommonJS run by Electron; evaluate it with a stand-in for 'electron'.
    const loadPreload = webUtils => {
        const source = readFileSync(join(process.cwd(), '../electron/preload.js'), 'utf8');
        let api;
        const electron = {
            contextBridge: { exposeInMainWorld: (name, value) => { api = value; } },
            ipcRenderer: { invoke: vi.fn(), on: vi.fn(), removeListener: vi.fn() },
            webUtils,
        };
        new Function('require', source)(name => (name === 'electron' ? electron : undefined));
        return api;
    };

    it('exposes only what main can do', () => {
        const api = loadPreload({ getPathForFile: () => '' });
        expect(Object.keys(api).sort()).toEqual([
            'getEngineToken', 'getPathForFile', 'getPreferences', 'onEngineStatus', 'selectAudioFile', 'setPreferences',
        ]);
    });

    it('answers \'\' instead of throwing when a file has no path', () => {
        const api = loadPreload({ getPathForFile: () => { throw new TypeError('not a File'); } });
        expect(api.getPathForFile({})).toBe('');
        expect(loadPreload({ getPathForFile: () => 'C:\\a.wav' }).getPathForFile({})).toBe('C:\\a.wav');
    });
});
