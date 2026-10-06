// Stage 10: the Preferences modal, the cog that opens it, and the preferences' owners.
import { describe, expect, it, vi } from 'vitest';
import { act, fireEvent, screen, waitFor, within } from '@testing-library/react';
import { ConfigProvider } from 'antd';
import TopBar from '../src/components/TopBar';
import PreferencesModal from '../src/components/preferences/PreferencesModal';
import { useKeyboardShortcuts } from '../src/hooks/useKeyboardShortcuts';
import { usePreferences } from '../src/hooks/usePreferences';
import { setPreferencesOpen } from '../src/store/appSlice';
import { setTransport } from '../src/store/playbackSlice';
import { selectFrameLength, selectFrameSmoothing, selectHopSize, selectSampleRate } from '../src/store/pipelineSlice';
import { APP_VERSION, BUILD_DATE, REPO_URL, formatBuildDate } from '../src/utils/appInfo';
import { formatAnalysisRange, formatFrame, formatHop, formatSmoothing } from '../src/utils/format';
import { makeAxis } from '../src/utils/frequencyAxis';
import { LIMITS } from '../src/utils/protocol';
import { FakeEngine, flush, makeStore, openTrack, renderWithEngine } from './helpers';

const DEFAULTS = { sampleRate: 8000, frameLength: 4096, hopSize: 64, frameSmoothing: 4 };

// What App mounts that matters here: the top bar, the modal, the shortcuts and the preferences hook.
const Shell = () => {
    useKeyboardShortcuts();
    usePreferences();
    return <><TopBar /><PreferencesModal /></>;
};

// jsdom runs no CSS animations, and antd's modal moves focus in and back only when its zoom has
// ended, so motion is off here.
const Harness = () => (
    <ConfigProvider theme={{ token: { motion: false } }}>
        <Shell />
    </ConfigProvider>
);

// The engine answers setEngineParams with every value, as it does on the wire.
const engineWithParams = (overrides = {}) => new FakeEngine({
    setEngineParams: args => ({ ok: true, value: { ...DEFAULTS, ...args } }),
    ...overrides,
});

const setup = ({ engine = engineWithParams(), state = 'playing', analysis = DEFAULTS } = {}) => {
    const rendered = renderWithEngine(<Harness />, { engine });
    openTrack(rendered.store, { state });
    act(() => rendered.store.dispatch(setTransport({ analysis })));
    return rendered;
};

const cog = () => screen.getByRole('button', { name: 'Preferences (Ctrl+,)' });
const dialog = () => screen.queryByRole('dialog', { name: 'Preferences' });
const chip = (group, name) => within(screen.getByRole('group', { name: group })).getByRole('button', { name });
const selected = group => within(screen.getByRole('group', { name: group }))
    .getAllByRole('button').filter(button => button.getAttribute('aria-pressed') === 'true').map(button => button.textContent);

const openModal = () => {
    fireEvent.click(cog());
    expect(dialog()).toBeTruthy();
};

// fireEvent returns false when a handler called preventDefault.
const press = (key, init = {}, target = window) => {
    let notPrevented;
    act(() => {
        notPrevented = fireEvent.keyDown(target, { key, ...init });
    });
    return !notPrevented;
};

describe('the Preferences modal', () => {
    it('opens from the cog, closes on Esc, and puts focus back on the cog', async () => {
        const { store } = setup();
        expect(dialog()).toBeNull();
        cog().focus();
        openModal();
        // It opens a dialog; it isn't a toggle.
        expect(cog().getAttribute('aria-pressed')).toBeNull();
        expect(cog().getAttribute('aria-haspopup')).toBe('dialog');
        // The focus trap holds focus inside while it's open.
        await waitFor(() => expect(dialog().contains(document.activeElement)).toBe(true));
        fireEvent.keyDown(document.activeElement, { key: 'Escape', keyCode: 27 });
        expect(store.getState().app.preferencesOpen).toBe(false);
        await waitFor(() => expect(dialog()).toBeNull());
        expect(document.activeElement).toBe(cog());
    });

    it('closes on Done and on the close button', async () => {
        const { store } = setup();
        openModal();
        fireEvent.click(screen.getByRole('button', { name: 'Done' }));
        expect(store.getState().app.preferencesOpen).toBe(false);
        openModal();
        fireEvent.click(screen.getByRole('button', { name: 'Close' }));
        expect(store.getState().app.preferencesOpen).toBe(false);
    });

    it('opens with Ctrl+, on the empty screen too', () => {
        const { store } = renderWithEngine(<Harness />);
        expect(press(',', { ctrlKey: true })).toBe(true);
        expect(store.getState().app.preferencesOpen).toBe(true);
        expect(dialog()).toBeTruthy();
    });

    it('suspends the other shortcuts while it is open, and lets Space press a chip', async () => {
        const selectAudioFile = vi.fn().mockResolvedValue(null);
        window.api = { selectAudioFile };
        const { engine, store } = setup({ state: 'paused' });
        act(() => store.dispatch(setPreferencesOpen(true)));
        for (const [key, init] of [[' '], ['ArrowRight'], ['.'], ['o', { ctrlKey: true }]])
            expect(press(key, init)).toBe(false);
        let keyUpNotPrevented;
        act(() => { keyUpNotPrevented = fireEvent.keyUp(chip('Hop size', '128'), { key: ' ' }); });
        expect(keyUpNotPrevented).toBe(true);
        await flush();
        expect(engine.calls).toEqual([]);
        expect(selectAudioFile).not.toHaveBeenCalled();
    });

    it('sends a hop size, and moves the chip only when the transport event says so', async () => {
        const { engine, store } = setup();
        openModal();
        expect(selected('Hop size')).toEqual(['64']);
        fireEvent.click(chip('Hop size', '128'));
        await flush();
        expect(engine.sent('setEngineParams').map(call => call.args)).toEqual([{ hopSize: 128 }]);
        expect(selected('Hop size')).toEqual(['64']);
        act(() => store.dispatch(setTransport({ analysis: { ...DEFAULTS, hopSize: 128 } })));
        expect(selected('Hop size')).toEqual(['128']);
        expect(screen.getByText('16 ms · 62.5 frames/s')).toBeTruthy();
    });

    describe('spectrum smoothing', () => {
        const count = () => screen.getByRole('spinbutton', { name: 'Spectrum smoothing' });
        const more = () => screen.getByRole('button', { name: 'Increase Spectrum smoothing' });
        const fewer = () => screen.getByRole('button', { name: 'Decrease Spectrum smoothing' });

        it('sends a count, saves the engine\'s answer, and moves only when the transport event says so', async () => {
            const setPreferences = vi.fn(async patch => ({ ok: true, value: { theme: 'system', ...patch } }));
            window.api = { setPreferences };
            const { engine, store } = setup();
            openModal();
            expect(count().getAttribute('aria-valuenow')).toBe('4');
            expect(screen.getByText('last 32 ms')).toBeTruthy();
            fireEvent.click(more());
            await flush();
            expect(engine.sent('setEngineParams').map(call => call.args)).toEqual([{ frameSmoothing: 5 }]);
            expect(setPreferences).toHaveBeenCalledWith({ engine: { ...DEFAULTS, frameSmoothing: 5 } });
            expect(count().getAttribute('aria-valuenow')).toBe('4');
            act(() => store.dispatch(setTransport({ analysis: { ...DEFAULTS, frameSmoothing: 5 } })));
            expect(count().getAttribute('aria-valuenow')).toBe('5');
            expect(screen.getByText('last 40 ms')).toBeTruthy();
        });

        it('steps from the value in flight when pressed quickly', async () => {
            const { engine } = setup();
            openModal();
            fireEvent.click(fewer());
            fireEvent.click(fewer());
            await flush();
            expect(engine.sent('setEngineParams').map(call => call.args))
                .toEqual([{ frameSmoothing: 3 }, { frameSmoothing: 2 }]);
        });

        it('stops at protocol.json\'s limits, and reads 1 as off', async () => {
            const { engine, store } = setup({ analysis: { ...DEFAULTS, frameSmoothing: LIMITS.frameSmoothingMax } });
            openModal();
            expect(more().disabled).toBe(true);
            expect(fewer().disabled).toBe(false);
            fireEvent.keyDown(count(), { key: 'ArrowUp' });
            await flush();
            expect(engine.sent('setEngineParams')).toEqual([]);

            act(() => store.dispatch(setTransport({ analysis: { ...DEFAULTS, frameSmoothing: LIMITS.frameSmoothingMin } })));
            expect(fewer().disabled).toBe(true);
            expect(screen.getByText('off')).toBeTruthy();
        });

        it('takes the arrow keys, Home and End', async () => {
            const { engine } = setup();
            openModal();
            for (const key of ['ArrowUp', 'ArrowDown', 'Home', 'End'])
                expect(fireEvent.keyDown(count(), { key })).toBe(false);
            await flush();
            expect(engine.sent('setEngineParams').map(call => call.args.frameSmoothing))
                .toEqual([5, 4, LIMITS.frameSmoothingMin, LIMITS.frameSmoothingMax]);
        });

        it('keeps its value when the engine refuses, and says why', async () => {
            const engine = engineWithParams({ setEngineParams: () => ({ ok: false, error: 'frameSmoothing is an integer from 1 to 10.' }) });
            const { store } = setup({ engine });
            openModal();
            fireEvent.click(more());
            await flush();
            expect(count().getAttribute('aria-valuenow')).toBe('4');
            expect(store.getState().app.statusMessage).toEqual({ type: 'error', message: 'frameSmoothing is an integer from 1 to 10.' });
        });

        const type = text => fireEvent.change(count(), { target: { value: text } });

        it('sends a typed value on Enter, and shows it only once the engine answers', async () => {
            const { engine, store } = setup();
            openModal();
            type('7');
            expect(count().value).toBe('7');
            expect(engine.sent('setEngineParams')).toEqual([]);
            fireEvent.keyDown(count(), { key: 'Enter' });
            await flush();
            expect(engine.sent('setEngineParams').map(call => call.args)).toEqual([{ frameSmoothing: 7 }]);
            expect(count().getAttribute('aria-valuenow')).toBe('4');
            act(() => store.dispatch(setTransport({ analysis: { ...DEFAULTS, frameSmoothing: 7 } })));
            expect(count().value).toBe('7');
            expect(count().getAttribute('aria-valuenow')).toBe('7');
        });

        it('clamps a typed value into the range', async () => {
            const { engine } = setup();
            openModal();
            for (const text of ['0', '99']) {
                type(text);
                fireEvent.keyDown(count(), { key: 'Enter' });
            }
            await flush();
            expect(engine.sent('setEngineParams').map(call => call.args.frameSmoothing))
                .toEqual([LIMITS.frameSmoothingMin, LIMITS.frameSmoothingMax]);
        });

        it('takes digits only, no more than the maximum has, and sends on leaving the field', async () => {
            const { engine } = setup();
            openModal();
            type('a-3.x');
            expect(count().value).toBe('3');
            type('1234');
            expect(count().value).toBe('1234'.slice(0, String(LIMITS.frameSmoothingMax).length));
            type('6');
            fireEvent.blur(count());
            await flush();
            expect(engine.sent('setEngineParams').map(call => call.args)).toEqual([{ frameSmoothing: 6 }]);
        });

        it('sends nothing for an empty field or the value already in force, and puts the value back', async () => {
            const { engine } = setup();
            openModal();
            type('');
            fireEvent.keyDown(count(), { key: 'Enter' });
            expect(count().value).toBe('4');
            type('4');
            fireEvent.blur(count());
            await flush();
            expect(engine.sent('setEngineParams')).toEqual([]);
        });

        it('steps from the typed value with the arrows', async () => {
            const { engine } = setup();
            openModal();
            type('8');
            fireEvent.keyDown(count(), { key: 'ArrowUp' });
            await flush();
            expect(engine.sent('setEngineParams').map(call => call.args)).toEqual([{ frameSmoothing: 9 }]);
        });

        it('cancels an edit on Esc without closing the dialog, and closes it on the next', async () => {
            const { engine, store } = setup();
            openModal();
            await waitFor(() => expect(dialog().contains(document.activeElement)).toBe(true));
            count().focus();
            type('9');
            fireEvent.keyDown(count(), { key: 'Escape', keyCode: 27 });
            expect(count().value).toBe('4');
            expect(store.getState().app.preferencesOpen).toBe(true);
            fireEvent.blur(count());
            await flush();
            expect(engine.sent('setEngineParams')).toEqual([]);
            fireEvent.keyDown(count(), { key: 'Escape', keyCode: 27 });
            expect(store.getState().app.preferencesOpen).toBe(false);
        });

        it('is disabled until the engine has sent its values', () => {
            renderWithEngine(<Harness />);
            openModal();
            expect(count().disabled).toBe(true);
            expect(more().disabled).toBe(true);
        });
    });

    it('saves the engine\'s answer, not the click, and only after an ok reply', async () => {
        const setPreferences = vi.fn(async patch => ({ ok: true, value: { theme: 'system', ...patch } }));
        window.api = { setPreferences };
        const engine = engineWithParams({
            setEngineParams: args => args.frameLength === 2048
                ? { ok: false, error: 'frameLength is one of 2048, 4096 or 8192.' }
                : { ok: true, value: { ...DEFAULTS, ...args } },
        });
        const { store } = setup({ engine });
        openModal();
        fireEvent.click(chip('Frame size', '8192'));
        await flush();
        expect(setPreferences).toHaveBeenCalledWith({ engine: { ...DEFAULTS, frameLength: 8192 } });

        fireEvent.click(chip('Frame size', '2048'));
        await flush();
        expect(setPreferences).toHaveBeenCalledTimes(1);
        expect(store.getState().app.statusMessage).toEqual({ type: 'error', message: 'frameLength is one of 2048, 4096 or 8192.' });
        expect(selected('Frame size')).toEqual(['4096']);
    });

    it('asks for a frame after a change while paused, as the tuning control does', async () => {
        const { engine } = setup({ state: 'paused' });
        openModal();
        fireEvent.click(chip('Sample rate', '16 kHz'));
        await flush();
        expect(engine.sent('setEngineParams').map(call => call.args)).toEqual([{ sampleRate: 16000 }]);
        expect(engine.sent('analyseFrame')).toHaveLength(1);
    });

    it('holds the sample rate chips until the engine has preprocessed the track', async () => {
        let answer;
        const engine = engineWithParams({ setEngineParams: () => new Promise(resolve => { answer = resolve; }) });
        setup({ engine });
        openModal();
        fireEvent.click(chip('Sample rate', '16 kHz'));
        await flush();
        expect(chip('Sample rate', '4 kHz').disabled).toBe(true);
        expect(chip('Hop size', '32').disabled).toBe(false);
        await act(async () => answer({ ok: true, value: { ...DEFAULTS, sampleRate: 16000 } }));
        expect(chip('Sample rate', '4 kHz').disabled).toBe(false);
    });

    it('disables the analysis rows until the engine has sent its values', () => {
        renderWithEngine(<Harness />);
        act(() => screen.getByRole('button', { name: 'Preferences (Ctrl+,)' }).click());
        expect(chip('Hop size', '64').disabled).toBe(true);
        expect(chip('Theme', 'Dark').disabled).toBe(false);
    });

    it('changes the theme only on main\'s ok', async () => {
        const answers = [{ ok: false, error: 'Couldn\'t save preferences: disk full' }, { ok: true, value: { theme: 'light', engine: {} } }];
        const setPreferences = vi.fn(async () => answers.shift());
        window.api = { setPreferences };
        const { store } = setup();
        openModal();
        fireEvent.click(chip('Theme', 'Light'));
        await flush();
        expect(setPreferences).toHaveBeenCalledWith({ theme: 'light' });
        expect(store.getState().settings.themeMode).toBe('system');
        expect(store.getState().app.statusMessage.message).toBe('Couldn\'t save preferences: disk full');
        fireEvent.click(chip('Theme', 'Light'));
        await flush();
        expect(store.getState().settings.themeMode).toBe('light');
        expect(selected('Theme')).toEqual(['Light']);
    });

    it('switches this window alone outside Electron, where there is no main to save it', async () => {
        const { store } = setup();
        openModal();
        fireEvent.click(chip('Theme', 'Dark'));
        await flush();
        expect(store.getState().settings.themeMode).toBe('dark');
    });

    it('takes the saved theme from main on mount', async () => {
        window.api = { getPreferences: vi.fn(async () => ({ ok: true, value: { theme: 'light', engine: {} } })) };
        const { store } = renderWithEngine(<Harness />);
        await flush();
        expect(store.getState().settings.themeMode).toBe('light');
    });

    it('restores the defaults: System and protocol.json\'s analysis values', async () => {
        const setPreferences = vi.fn(async patch => ({ ok: true, value: { theme: 'system', engine: {}, ...patch } }));
        window.api = { setPreferences };
        const { engine } = setup({ analysis: { sampleRate: 16000, frameLength: 8192, hopSize: 256 } });
        openModal();
        fireEvent.click(screen.getByRole('button', { name: 'Restore defaults' }));
        await flush();
        expect(setPreferences).toHaveBeenCalledWith({ theme: 'system' });
        expect(engine.sent('setEngineParams').map(call => call.args)).toEqual([{
            sampleRate: LIMITS.sampleRateDefault,
            frameLength: LIMITS.frameLengthDefault,
            hopSize: LIMITS.hopSizeDefault,
            frameSmoothing: LIMITS.frameSmoothingDefault,
        }]);
    });

    it('shows the version, the build date and the repo link, opened outside the app', () => {
        setup();
        openModal();
        expect(screen.getByText(new RegExp(`Version ${APP_VERSION.replace(/\./g, '\\.')}`))).toBeTruthy();
        expect(screen.getByText(new RegExp(`Updated ${formatBuildDate(BUILD_DATE)}`))).toBeTruthy();
        const link = screen.getByRole('link', { name: /github\.com\/Seank23\/Adagio-Electron/ });
        expect(link.getAttribute('href')).toBe(REPO_URL);
        expect(link.getAttribute('target')).toBe('_blank');
    });
});

describe('preferences state and readouts', () => {
    it('takes the engine\'s analysis parameters from a transport event, and keeps them when one has none', () => {
        const store = makeStore();
        const params = () => [selectSampleRate, selectFrameLength, selectHopSize, selectFrameSmoothing]
            .map(select => select(store.getState()));
        expect(params()).toEqual([null, null, null, null]);
        store.dispatch(setTransport({ state: 'empty', track: null, analysis: { sampleRate: 16000, frameLength: 8192, hopSize: 32, frameSmoothing: 6 } }));
        expect(params()).toEqual([16000, 8192, 32, 6]);
        store.dispatch(setTransport({ state: 'ready' }));
        expect(params()).toEqual([16000, 8192, 32, 6]);
    });

    it('formats the readouts from the rate as well as their own value', () => {
        expect(formatAnalysisRange(8000)).toBe('up to 4 kHz');
        expect(formatAnalysisRange(16000)).toBe('up to 8 kHz');
        expect(formatHop(64, 8000)).toBe('8 ms · 125 frames/s');
        expect(formatHop(64, 16000)).toBe('4 ms · 250 frames/s');
        expect(formatHop(256, 4000)).toBe('64 ms · 15.6 frames/s');
        expect(formatFrame(4096, 8000)).toBe('1.95 Hz per bin · 512 ms');
        expect(formatFrame(4096, 16000)).toBe('3.91 Hz per bin · 256 ms');
        expect(formatSmoothing(4, 128, 8000)).toBe('last 64 ms');
        expect(formatSmoothing(10, 256, 4000)).toBe('last 640 ms');
        expect(formatSmoothing(3, 32, 16000)).toBe('last 6 ms');
        expect(formatSmoothing(1, 128, 8000)).toBe('off');
    });

    it('formats the build date as a day, in any time zone', () => {
        expect(formatBuildDate('2026-10-05')).toBe('5 October 2026');
        expect(BUILD_DATE).toMatch(/^\d{4}-\d{2}-\d{2}$/);
        expect(APP_VERSION).toMatch(/^\d+\.\d+\.\d+$/);
    });

    it('labels the log axis up to 5 kHz when it runs to 8 kHz', () => {
        expect(makeAxis({ maxHz: 8000, width: 800, log: true }).ticks).toContain(5000);
        expect(makeAxis({ maxHz: 4000, width: 800, log: true }).ticks).not.toContain(5000);
    });
});
