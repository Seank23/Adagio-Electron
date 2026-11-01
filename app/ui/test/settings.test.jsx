// Stage 11: the sidebar's Settings tab, the pipeline state it reads and the rules its controls follow.
import { Profiler } from 'react';
import { describe, expect, it } from 'vitest';
import { act, fireEvent, screen, within } from '@testing-library/react';
import { ConfigProvider } from 'antd';
import Sidebar from '../src/components/sidebar/Sidebar';
import SettingsTab from '../src/components/sidebar/settings/SettingsTab';
import { setAnalysisData } from '../src/store/analysisSlice';
import { setTransport } from '../src/store/playbackSlice';
import {
    selectActivePipeline, selectHasChanges, selectPipelines, selectStages, selectTuningHz, setSchema, setSettingValue,
} from '../src/store/pipelineSlice';
import { setSidebarTab } from '../src/store/settingsSlice';
import {
    controlKind, formatSetting, formatStageName, isChanged, normaliseSetting, parseSetting, sliderRange, stepOf,
} from '../src/utils/settings';
import { FakeEngine, analysisEvent, flush, makeStore, openTrack, renderWithEngine } from './helpers';

// Schema entries as AnalysisPipeline::GetSchemaJson sends them. A float default comes from a float in
// the stage's TSettings, so it arrives widened: 0.2f is 0.20000000298023224.
const f32 = Math.fround;
const number = (key, name, type, min, max, value, extra = {}) => ({ key, name, type, min, max, default: value, value, ...extra });
const choice = (key, name, options, value) => ({ key, name, type: 'enum', options, default: value, value });
const stage = (name, settings) => ({ name, type: 'processor', settings });

const PIPELINES = [
    { id: 'hps', name: 'Harmonic Product Spectrum', description: 'The original pipeline: multiplies downsampled spectra so fundamentals stand out, then picks peaks.' },
    { id: 'partialTracking', name: 'Partial Tracking', description: 'Finds partials, groups them into harmonic series and picks notes by salience.' },
];

const SHARED_HEAD = [
    stage('FFTProcessor', [choice('WINDOW', 'Window Function', ['Rectangle', 'Hamming', 'Hann', 'BlackmannHarris'], 'Hamming')]),
    stage('SpectrumFilterProcessor', [
        choice('ENABLED', 'Enabled', ['Yes', 'No'], 'Yes'),
        number('LOW_CUT_C1', 'Low Cut C1', 'float', 0, 20000, 55),
    ]),
];
const SHARED_TAIL = [
    stage('PresenceFilter', [
        choice('MASK_ENABLED', 'Mask Enabled', ['No', 'Yes'], 'No'),
        number('PRESENCE_WINDOW', 'Presence Window', 'float', 0, 1, f32(0.2)),
    ]),
    stage('NoteDetector', [
        number('A440_TUNING', 'A440 Reference', 'float', 415, 466, 440, { persistent: false }),
        number('ERROR_THRESHOLD', 'Error Threshold', 'float', 0, 50, 25),
    ]),
    stage('KeyDetector', [
        choice('ALGORITHM', 'Algorithm', ['Scale Match', 'Krumhansl'], 'Krumhansl'),
        number('ROLLING_WINDOW', 'Rolling Window', 'float', 5, 50, 20),
    ]),
    stage('ChordPredictor', [
        number('ROLLING_WINDOW', 'Rolling Window', 'float', 0.01, 5, 0.5),
        number('SCORE_THRESHOLD', 'Score Threshold', 'float', 0, 1, f32(0.1)),
    ]),
];

const HPS = {
    version: 2,
    activePipeline: 'hps',
    pipelines: PIPELINES,
    stages: [
        ...SHARED_HEAD,
        stage('HPSDownsamplerProcessor', [
            number('HARMONICS', 'Number of Harmonics', 'int', 0, 5, 2),
            choice('NORMALIZE', 'Normalize Output', ['Before Downsampling', 'After Downsampling', 'No'], 'No'),
        ]),
        stage('PeakExtractor', [number('MIN_SNR', 'Min SNR Above Local Floor', 'float', 0.001, 30, f32(0.2))]),
        ...SHARED_TAIL,
    ],
};

const PARTIAL_TRACKING = {
    ...HPS,
    activePipeline: 'partialTracking',
    stages: [
        ...SHARED_HEAD,
        stage('PartialExtractor', [number('GATE_DB', 'Gate Level dB', 'float', 0, 40, 4)]),
        stage('HarmonicGrouper', [number('MATCH_CENTS', 'Match Cents', 'int', 5, 50, 25)]),
        stage('NoteSelector', [number('STOP_DB', 'Stop dB', 'float', 10, 80, 30)]),
        ...SHARED_TAIL,
    ],
};

const findSetting = (schema, stageName, key) =>
    schema.stages.find(s => s.name === stageName).settings.find(s => s.key === key);

// An answer the test sends when it chooses, to see what shows while a command is in flight.
const deferred = () => {
    let resolve;
    const promise = new Promise(done => { resolve = done; });
    return { promise, resolve };
};

const Harness = ({ children }) => <ConfigProvider theme={{ token: { motion: false } }}>{children}</ConfigProvider>;

const setup = ({ handlers = {}, state = 'paused', schema = HPS, ui = <SettingsTab /> } = {}) => {
    const engine = new FakeEngine(handlers);
    const rendered = renderWithEngine(<Harness>{ui}</Harness>, { engine });
    openTrack(rendered.store, { state });
    if (schema)
        act(() => rendered.store.dispatch(setSchema(schema)));
    return rendered;
};

const header = name => screen.getByRole('button', { name });
const headers = () => screen.getAllByRole('button', { expanded: false }).concat(screen.queryAllByRole('button', { expanded: true }));
const region = name => screen.getByRole('region', { name });
const open = name => fireEvent.click(header(name));
const pipelineSelect = () => screen.getByRole('combobox', { name: 'Analysis pipeline' });
// What the closed select shows: antd puts the chosen option's label in a titled element beside the input.
const shownPipeline = () => pipelineSelect().closest('.ant-select').querySelector('[title]')?.textContent;
const resetAll = () => screen.getByRole('button', { name: 'Reset all' });

// Types into a number field and presses Enter.
const type = (field, text) => {
    fireEvent.focus(field);
    fireEvent.change(field, { target: { value: text } });
    fireEvent.keyDown(field, { key: 'Enter' });
};

describe('utils/settings: the rules the controls follow', () => {
    const kind = (stageName, key, schema = HPS) => controlKind(findSetting(schema, stageName, key));

    it('picks a control from the schema entry alone', () => {
        expect(kind('SpectrumFilterProcessor', 'ENABLED')).toBe('switch');
        expect(kind('PresenceFilter', 'MASK_ENABLED')).toBe('switch');
        expect(kind('KeyDetector', 'ALGORITHM')).toBe('segmented');
        expect(kind('HPSDownsamplerProcessor', 'NORMALIZE')).toBe('select');
        expect(kind('FFTProcessor', 'WINDOW')).toBe('select');
        expect(kind('HPSDownsamplerProcessor', 'HARMONICS')).toBe('number');
        expect(kind('NoteDetector', 'A440_TUNING')).toBe('number');
        // Two options, but too long for chips.
        expect(controlKind(choice('X', 'X', ['Before Downsampling', 'No'], 'No'))).toBe('select');
    });

    it('compares a widened float default within a millionth', () => {
        const presence = findSetting(HPS, 'PresenceFilter', 'PRESENCE_WINDOW');
        expect(presence.default).not.toBe(0.2);
        expect(isChanged({ ...presence, value: 0.2 })).toBe(false);
        expect(isChanged({ ...presence, value: 0.21 })).toBe(true);
        expect(isChanged({ ...findSetting(HPS, 'NoteDetector', 'A440_TUNING'), value: 442 })).toBe(true);
        expect(isChanged({ ...findSetting(HPS, 'FFTProcessor', 'WINDOW'), value: 'Hann' })).toBe(true);
        expect(isChanged(findSetting(HPS, 'FFTProcessor', 'WINDOW'))).toBe(false);
    });

    it('steps by 1, 2 or 5 × 10ⁿ under a hundredth of the range, and 1 for an int', () => {
        const step = (stageName, key) => stepOf(findSetting(HPS, stageName, key));
        expect(step('NoteDetector', 'A440_TUNING')).toBe(0.5);
        expect(step('ChordPredictor', 'ROLLING_WINDOW')).toBe(0.02);
        expect(step('PresenceFilter', 'PRESENCE_WINDOW')).toBe(0.01);
        expect(step('SpectrumFilterProcessor', 'LOW_CUT_C1')).toBe(200);
        expect(step('KeyDetector', 'ROLLING_WINDOW')).toBe(0.2);
        expect(step('PeakExtractor', 'MIN_SNR')).toBe(0.2);
        expect(step('HPSDownsamplerProcessor', 'HARMONICS')).toBe(1);
    });

    it('formats with the step\'s decimals, and more for a value off that grid', () => {
        const format = (stageName, key, value) => formatSetting(findSetting(HPS, stageName, key), value);
        expect(format('NoteDetector', 'A440_TUNING', 442)).toBe('442.0');
        expect(format('ChordPredictor', 'ROLLING_WINDOW', 0.5)).toBe('0.50');
        expect(format('SpectrumFilterProcessor', 'LOW_CUT_C1', 55)).toBe('55');
        expect(format('PresenceFilter', 'PRESENCE_WINDOW', f32(0.2))).toBe('0.20');
        expect(format('PeakExtractor', 'MIN_SNR', 0.001)).toBe('0.001');
        expect(format('HPSDownsamplerProcessor', 'HARMONICS', 2)).toBe('2');
        expect(format('FFTProcessor', 'WINDOW', 'Hann')).toBe('Hann');
    });

    it('puts the slider\'s ends on its step\'s grid', () => {
        expect(sliderRange(findSetting(HPS, 'ChordPredictor', 'ROLLING_WINDOW'))).toEqual({ min: 0.02, max: 5, step: 0.02 });
        expect(sliderRange(findSetting(HPS, 'PeakExtractor', 'MIN_SNR'))).toEqual({ min: 0.2, max: 30, step: 0.2 });
        expect(sliderRange(findSetting(HPS, 'NoteDetector', 'A440_TUNING'))).toEqual({ min: 415, max: 466, step: 0.5 });
    });

    it('reads typed text as the setting\'s type, clamped to its range', () => {
        const harmonics = findSetting(HPS, 'HPSDownsamplerProcessor', 'HARMONICS');
        expect(parseSetting(harmonics, '4.6')).toBe(5);
        expect(parseSetting(harmonics, '99')).toBe(5);
        expect(parseSetting(harmonics, '')).toBeNull();
        expect(parseSetting(harmonics, 'many')).toBeNull();
        const tuning = findSetting(HPS, 'NoteDetector', 'A440_TUNING');
        expect(parseSetting(tuning, ' 442.25 ')).toBe(442.25);
        expect(parseSetting(tuning, '400')).toBe(415);
        expect(normaliseSetting(findSetting(HPS, 'PresenceFilter', 'PRESENCE_WINDOW'), 0.1 + 0.2)).toBe(0.3);
    });

    it('names every stage the engine has from its settings key', () => {
        expect([
            'FFTProcessor', 'SpectrumFilterProcessor', 'HPSDownsamplerProcessor', 'PeakExtractor', 'PartialExtractor',
            'HarmonicGrouper', 'NoteSelector', 'PresenceFilter', 'NoteDetector', 'KeyDetector', 'ChordPredictor',
        ].map(formatStageName)).toEqual([
            'FFT', 'Spectrum Filter', 'HPS Downsampler', 'Peak Extractor', 'Partial Extractor',
            'Harmonic Grouper', 'Note Selector', 'Presence Filter', 'Note Detector', 'Key Detector', 'Chord Predictor',
        ]);
    });
});

describe('pipelineSlice: the pipelines are the engine\'s', () => {
    it('takes the active pipeline and the list from the schema, and defaults them without one', () => {
        const store = makeStore();
        expect([selectActivePipeline(store.getState()), selectPipelines(store.getState())]).toEqual([null, []]);
        store.dispatch(setSchema(HPS));
        expect(selectActivePipeline(store.getState())).toBe('hps');
        expect(selectPipelines(store.getState()).map(pipeline => pipeline.id)).toEqual(['hps', 'partialTracking']);
        expect(selectStages(store.getState())).toHaveLength(8);
        store.dispatch(setSchema({ stages: [] }));
        expect([selectActivePipeline(store.getState()), selectPipelines(store.getState())]).toEqual([null, []]);
    });

    it('says whether anything is off its default, and keeps the stages through a transport event', () => {
        const store = makeStore();
        store.dispatch(setSchema(HPS));
        expect(selectHasChanges(store.getState())).toBe(false);
        const stages = selectStages(store.getState());
        store.dispatch(setTransport({ analysis: { sampleRate: 8000, frameLength: 4096, hopSize: 128, frameSmoothing: 4 } }));
        expect(selectStages(store.getState())).toBe(stages);

        store.dispatch(setSettingValue({ stage: 'NoteDetector', key: 'A440_TUNING', value: 442 }));
        expect(selectHasChanges(store.getState())).toBe(true);
        // immer keeps every other stage the same object, which is what lets their sections skip a render.
        const next = selectStages(store.getState());
        expect(next.filter((s, i) => s === stages[i]).map(s => s.name)).not.toContain('NoteDetector');
        expect(next.filter((s, i) => s === stages[i])).toHaveLength(7);
    });
});

describe('the Settings tab', () => {
    it('shows one collapsed header per stage, numbered in pipeline order, under the pipeline in force', () => {
        setup();
        const stageHeaders = screen.getAllByRole('button', { expanded: false });
        expect(stageHeaders.map(button => button.textContent)).toEqual([
            '01FFT', '02Spectrum Filter', '03HPS Downsampler', '04Peak Extractor',
            '05Presence Filter', '06Note Detector', '07Key Detector', '08Chord Predictor',
        ]);
        expect(screen.queryAllByRole('region')).toEqual([]);
        expect(screen.getByText('8 stages · changes apply immediately')).toBeTruthy();
        expect(shownPipeline()).toBe('Harmonic Product Spectrum');
    });

    it('is disabled until the schema arrives, and while the engine is away', () => {
        setup({ schema: null });
        expect(pipelineSelect().disabled).toBe(true);
        expect(resetAll().disabled).toBe(true);
        expect(screen.getByText('Waiting for the engine')).toBeTruthy();
    });

    it('opens a stage from its header, and keeps it open across a tab switch and a pipeline switch', async () => {
        const { store } = setup({ ui: <Sidebar />, handlers: { switchPipeline: () => ({ ok: true, value: PARTIAL_TRACKING }) } });
        act(() => store.dispatch(setSidebarTab('settings')));
        open('Key Detector');
        expect(header('Key Detector').getAttribute('aria-expanded')).toBe('true');
        expect(within(region('Key Detector')).getByRole('spinbutton', { name: 'Rolling Window' })).toBeTruthy();

        act(() => store.dispatch(setSidebarTab('analysis')));
        expect(screen.queryByRole('region', { name: 'Key Detector' })).toBeNull();
        act(() => store.dispatch(setSidebarTab('settings')));
        expect(region('Key Detector')).toBeTruthy();

        fireEvent.mouseDown(pipelineSelect());
        fireEvent.click(await screen.findByText('Partial Tracking', { selector: 'span' }));
        await flush();
        expect(header('Key Detector').getAttribute('aria-expanded')).toBe('true');
        open('Key Detector');
        expect(screen.queryByRole('region', { name: 'Key Detector' })).toBeNull();
    });

    it('sends a typed value, and shows it as the engine\'s only on ok', async () => {
        const answer = deferred();
        const { engine, store } = setup({ handlers: { setAnalysisSetting: () => answer.promise } });
        open('Key Detector');
        const field = within(region('Key Detector')).getByRole('spinbutton', { name: 'Rolling Window' });
        type(field, '30');
        await flush();
        expect(engine.sent('setAnalysisSetting').map(call => call.args)).toEqual([{ stage: 'KeyDetector', key: 'ROLLING_WINDOW', value: 30 }]);
        // In flight: the field holds what was typed, but nothing says it's changed yet.
        expect(field.value).toBe('30.0');
        expect(findSetting({ stages: selectStages(store.getState()) }, 'KeyDetector', 'ROLLING_WINDOW').value).toBe(20);
        expect(screen.queryByRole('button', { name: 'Reset Rolling Window' })).toBeNull();

        await act(async () => answer.resolve({ ok: true, value: null }));
        await flush();
        expect(findSetting({ stages: selectStages(store.getState()) }, 'KeyDetector', 'ROLLING_WINDOW').value).toBe(30);
        expect(field.value).toBe('30.0');
        expect(screen.getByRole('button', { name: 'Reset Rolling Window' })).toBeTruthy();
        expect(header('Key Detector').querySelector('[title="Changed from its defaults"]')).toBeTruthy();
        expect(resetAll().disabled).toBe(false);
    });

    it('puts the engine\'s value back when it refuses one, and says why', async () => {
        const { store } = setup({ handlers: { setAnalysisSetting: () => ({ ok: false, error: "Setting 'ERROR_THRESHOLD' should be between 0 and 50." }) } });
        open('Note Detector');
        const field = within(region('Note Detector')).getByRole('spinbutton', { name: 'Error Threshold' });
        fireEvent.keyDown(field, { key: 'ArrowUp' });
        await flush();
        expect(field.value).toBe('25.0');
        expect(store.getState().app.statusMessage).toEqual({ type: 'error', message: "Setting 'ERROR_THRESHOLD' should be between 0 and 50." });
    });

    it('steps the field by its step, ten at a time with Shift, inside the range', async () => {
        const { engine } = setup();
        open('Note Detector');
        const tuning = within(region('Note Detector')).getByRole('spinbutton', { name: 'A440 Reference' });
        fireEvent.keyDown(tuning, { key: 'ArrowUp' });
        await flush();
        fireEvent.keyDown(tuning, { key: 'ArrowDown', shiftKey: true });
        await flush();
        type(tuning, '500');
        await flush();
        expect(engine.sent('setAnalysisSetting').map(call => call.args.value)).toEqual([440.5, 435.5, 466]);
    });

    it('keeps the A4 field and the tuning control on one value', async () => {
        const { store } = setup();
        open('Note Detector');
        act(() => store.dispatch(setSettingValue({ stage: 'NoteDetector', key: 'A440_TUNING', value: 442 })));
        expect(within(region('Note Detector')).getByRole('spinbutton', { name: 'A440 Reference' }).value).toBe('442.0');
        expect(selectTuningHz(store.getState())).toBe(442);
    });

    it('analyses the paused frame after each accepted change, and not after a refused one or while playing', async () => {
        let refuse = false;
        const handlers = { setAnalysisSetting: () => (refuse ? { ok: false, error: 'No.' } : { ok: true, value: null }) };
        const { engine, store } = setup({ handlers });
        open('Spectrum Filter');
        fireEvent.click(screen.getByRole('switch', { name: 'Enabled' }));
        await flush();
        expect(engine.sent('setAnalysisSetting').map(call => call.args)).toEqual([{ stage: 'SpectrumFilterProcessor', key: 'ENABLED', value: 'No' }]);
        expect(engine.sent('analyseFrame')).toHaveLength(1);
        expect(screen.getByRole('switch', { name: 'Enabled' }).getAttribute('aria-checked')).toBe('false');

        refuse = true;
        fireEvent.click(screen.getByRole('switch', { name: 'Enabled' }));
        await flush();
        expect(engine.sent('analyseFrame')).toHaveLength(1);

        refuse = false;
        openTrack(store, { state: 'playing' });
        fireEvent.click(screen.getByRole('switch', { name: 'Enabled' }));
        await flush();
        expect(engine.sent('setAnalysisSetting')).toHaveLength(3);
        expect(engine.sent('analyseFrame')).toHaveLength(1);
    });

    it('sends a chip\'s option and a select\'s option', async () => {
        const { engine } = setup();
        open('Key Detector');
        fireEvent.click(within(screen.getByRole('group', { name: 'Algorithm' })).getByRole('button', { name: 'Scale Match' }));
        open('FFT');
        fireEvent.mouseDown(within(region('FFT')).getByRole('combobox', { name: 'Window Function' }));
        fireEvent.click(await screen.findByTitle('Hann'));
        await flush();
        expect(engine.sent('setAnalysisSetting').map(call => call.args)).toEqual([
            { stage: 'KeyDetector', key: 'ALGORITHM', value: 'Scale Match' },
            { stage: 'FFTProcessor', key: 'WINDOW', value: 'Hann' },
        ]);
    });

    it('sends the default from a changed setting\'s reset arrow, and shows the arrow only when changed', async () => {
        const { engine, store } = setup();
        open('Chord Predictor');
        expect(screen.queryByRole('button', { name: /^Reset (?!all)/ })).toBeNull();
        act(() => store.dispatch(setSettingValue({ stage: 'ChordPredictor', key: 'SCORE_THRESHOLD', value: 0.25 })));
        fireEvent.click(screen.getByRole('button', { name: 'Reset Score Threshold' }));
        await flush();
        expect(engine.sent('setAnalysisSetting').map(call => call.args)).toEqual([{ stage: 'ChordPredictor', key: 'SCORE_THRESHOLD', value: f32(0.1) }]);
        expect(screen.queryByRole('button', { name: 'Reset Score Threshold' })).toBeNull();
    });

    it('restores every default with one command, and dims Reset all when nothing is changed', async () => {
        const { engine, store } = setup({ handlers: { restorePipelineDefaults: () => ({ ok: true, value: HPS }) } });
        expect(resetAll().disabled).toBe(true);
        act(() => store.dispatch(setSettingValue({ stage: 'FFTProcessor', key: 'WINDOW', value: 'Hann' })));
        expect(resetAll().disabled).toBe(false);
        fireEvent.click(resetAll());
        await flush();
        expect(engine.sent('restorePipelineDefaults')).toHaveLength(1);
        expect(resetAll().disabled).toBe(true);
        expect(engine.sent('analyseFrame')).toHaveLength(1);
    });

    it('switches pipeline by id, and lists the new stages only when the engine answers', async () => {
        const answer = deferred();
        const { engine, store } = setup({ handlers: { switchPipeline: () => answer.promise } });
        fireEvent.mouseDown(pipelineSelect());
        // The menu has each pipeline's description; the one in force carries the check.
        expect(await screen.findByText(PIPELINES[1].description)).toBeTruthy();
        fireEvent.click(screen.getByText('Partial Tracking', { selector: 'span' }));
        await flush();
        expect(engine.sent('switchPipeline').map(call => call.args)).toEqual(['partialTracking']);
        expect(screen.getAllByRole('button', { expanded: false })).toHaveLength(8);
        expect(pipelineSelect().disabled).toBe(true);
        expect(selectActivePipeline(store.getState())).toBe('hps');

        await act(async () => answer.resolve({ ok: true, value: PARTIAL_TRACKING }));
        await flush();
        expect(screen.getAllByRole('button', { expanded: false }).map(button => button.textContent).slice(2, 5))
            .toEqual(['03Partial Extractor', '04Harmonic Grouper', '05Note Selector']);
        expect(screen.getByText('9 stages · changes apply immediately')).toBeTruthy();
        expect(pipelineSelect().disabled).toBe(false);
        expect(selectActivePipeline(store.getState())).toBe('partialTracking');
        expect(engine.sent('analyseFrame')).toHaveLength(1);
    });

    it('stays on the engine\'s pipeline when a switch is refused, and says why', async () => {
        const { store } = setup({ handlers: { switchPipeline: () => ({ ok: false, error: "There is no pipeline 'gone'." }) } });
        fireEvent.mouseDown(pipelineSelect());
        fireEvent.click(await screen.findByText('Partial Tracking', { selector: 'span' }));
        await flush();
        expect(selectActivePipeline(store.getState())).toBe('hps');
        expect(shownPipeline()).toBe('Harmonic Product Spectrum');
        expect(store.getState().app.statusMessage.type).toBe('error');
    });

    it('renders nothing for analysis or transport events', () => {
        let commits = 0;
        const { store } = setup({ ui: <Profiler id="settings" onRender={() => commits++}><SettingsTab /></Profiler>, state: 'playing' });
        open('Note Detector');
        const before = commits;
        for (let i = 0; i < 20; i++) {
            act(() => store.dispatch(setAnalysisData(analysisEvent({ avgTuningError: i / 10 }).value)));
            act(() => store.dispatch(setTransport({ analysis: { sampleRate: 8000, frameLength: 4096, hopSize: 128, frameSmoothing: 4 } })));
        }
        expect(commits).toBe(before);
        expect(headers()).toHaveLength(8);
    });
});
