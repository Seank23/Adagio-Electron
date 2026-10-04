import { describe, expect, it } from 'vitest';
import { makeStore, TRACK, analysisEvent } from './helpers';
import { setTransport, selectIsFileOpen, selectIsPaused, selectIsPlaying } from '../src/store/playbackSlice';
import {
    resetAnalysis, selectAnalysisRate, selectChordNamesKey, selectChordProbabilities, selectPitchClassPercents,
    setAnalysisData,
} from '../src/store/analysisSlice';
import {
    TUNING_KEY, TUNING_STAGE, selectHasSchema, selectTuningHz, selectTuningMax, selectTuningMin, setSchema,
    setSettingValue,
} from '../src/store/pipelineSlice';

const SCHEMA = {
    stages: [{
        name: TUNING_STAGE,
        settings: [
            { key: 'ERROR_THRESHOLD', type: 'float', value: 25 },
            { key: TUNING_KEY, type: 'float', min: 415, max: 466, value: 440 },
        ],
    }],
};

describe('playbackSlice: the engine owns the transport', () => {
    it('copies the track from a transport event and takes the duration from it', () => {
        const store = makeStore();
        store.dispatch(setTransport({ state: 'paused', track: TRACK, speed: 0.75, volume: 0.55 }));
        const { playback } = store.getState();
        expect(playback.track).toEqual(TRACK);
        expect(playback.duration).toBe(312);
        expect(playback.speed).toBe(0.75);
        expect(playback.volume).toBe(0.55);
    });

    it('clears the track on a null one, but keeps it when the key is absent', () => {
        const store = makeStore();
        store.dispatch(setTransport({ state: 'ready', track: TRACK }));
        store.dispatch(setTransport({ state: 'playing' }));
        expect(store.getState().playback.track).toEqual(TRACK);
        store.dispatch(setTransport({ state: 'empty', track: null }));
        expect(store.getState().playback.track).toBeNull();
        expect(store.getState().playback.duration).toBe(0);
    });

    it('derives playing, paused and file-open from the one state', () => {
        const store = makeStore();
        const flags = () => [selectIsFileOpen, selectIsPlaying, selectIsPaused].map(select => select(store.getState()));
        expect(flags()).toEqual([false, false, false]);
        store.dispatch(setTransport({ state: 'loading' }));
        expect(flags()).toEqual([false, false, false]);
        store.dispatch(setTransport({ state: 'playing' }));
        expect(flags()).toEqual([true, true, false]);
        store.dispatch(setTransport({ state: 'paused' }));
        expect(flags()).toEqual([true, false, true]);
    });

    it('starts at the engine default volume of 20%', () => {
        expect(makeStore().getState().playback.volume).toBe(0.2);
    });
});

describe('analysisSlice', () => {
    it('stores null for a key with no name: TonicClass is -1 and Scale is zeros until then', () => {
        const store = makeStore();
        store.dispatch(setAnalysisData(analysisEvent({
            key: { name: '', tonic: '', tonicClass: -1, mode: '', scaleClasses: [0, 0, 0, 0, 0, 0, 0] },
        }).value));
        expect(store.getState().analysis.detectedKey).toBeNull();
    });

    it('keeps the same key object while the name is unchanged, so its readers do not re-render', () => {
        const store = makeStore();
        store.dispatch(setAnalysisData(analysisEvent().value));
        const first = store.getState().analysis.detectedKey;
        store.dispatch(setAnalysisData(analysisEvent().value));
        expect(store.getState().analysis.detectedKey).toBe(first);
        store.dispatch(setAnalysisData(analysisEvent({
            key: { name: 'Eb Minor', tonic: 'Eb', tonicClass: 3, mode: 'Minor', scaleClasses: [6, 8, 10, 11, 1, 3, 5] },
        }).value));
        expect(store.getState().analysis.detectedKey.name).toBe('Eb Minor');
    });

    it('keeps the analysis rate through a reset, so the axis does not collapse', () => {
        const store = makeStore();
        store.dispatch(setAnalysisData(analysisEvent().value));
        store.dispatch(resetAnalysis());
        const { analysis } = store.getState();
        expect(analysis.spectrumSR).toBe(8000);
        expect(analysis.detectedKey).toBeNull();
        expect(analysis.predictedChords).toEqual([]);
        expect(analysis.keyHistogram).toEqual([]);
    });

    it('takes the axis rate from the track until the first analysis event', () => {
        const store = makeStore();
        expect(selectAnalysisRate(store.getState())).toBe(0);
        store.dispatch(setTransport({ state: 'ready', track: TRACK }));
        expect(selectAnalysisRate(store.getState())).toBe(8000);
        store.dispatch(setAnalysisData(analysisEvent({ sampleRate: 11025 }).value));
        expect(selectAnalysisRate(store.getState())).toBe(11025);
    });

    it('folds the key histogram by pitch class against the engine reference', () => {
        const store = makeStore();
        store.dispatch(setSchema(SCHEMA));
        // 452.9 Hz is A4 only once the reference is 452.9; at 440 it rounds to A as well,
        // but 466 Hz moves from Bb to A when the reference rises.
        store.dispatch(setAnalysisData(analysisEvent({
            keyHistogram: [{ frequency: 466.16, score: 3 }, { frequency: 261.63, score: 1 }, { frequency: 0, score: 9 }],
        }).value));
        let percents = selectPitchClassPercents(store.getState());
        expect(percents[10]).toBeCloseTo(75);
        expect(percents[0]).toBeCloseTo(25);
        expect(percents.reduce((sum, value) => sum + value, 0)).toBeCloseTo(100);

        store.dispatch(setSettingValue({ stage: TUNING_STAGE, key: TUNING_KEY, value: 466 }));
        percents = selectPitchClassPercents(store.getState());
        expect(percents[9]).toBeCloseTo(75);
    });

    it('gives the top four chord probabilities as fractions and the names as one string', () => {
        const store = makeStore();
        const chords = [['A', 0.4], ['B', 0.35], ['C', 0.3], ['D', 0.25], ['E', 0.2]].map(([name, probability]) => ({ name, probability }));
        store.dispatch(setAnalysisData(analysisEvent({ chords }).value));
        const probabilities = selectChordProbabilities(store.getState());
        expect(probabilities).toEqual([0.4, 0.35, 0.3, 0.25]);
        expect(probabilities.every(p => p <= 1)).toBe(true);
        expect(selectChordProbabilities(store.getState())).toBe(probabilities);
        expect(selectChordNamesKey(store.getState())).toBe('A|B|C|D');
    });
});

describe('pipelineSlice: the tuning reference is the engine\'s', () => {
    it('defaults to 440 and 415–466 before the schema arrives', () => {
        const state = makeStore().getState();
        expect(selectHasSchema(state)).toBe(false);
        expect(selectTuningHz(state)).toBe(440);
        expect([selectTuningMin(state), selectTuningMax(state)]).toEqual([415, 466]);
    });

    it('patches only a setting the schema has', () => {
        const store = makeStore();
        store.dispatch(setSchema(SCHEMA));
        store.dispatch(setSettingValue({ stage: TUNING_STAGE, key: TUNING_KEY, value: 442 }));
        store.dispatch(setSettingValue({ stage: TUNING_STAGE, key: 'NOT_A_SETTING', value: 1 }));
        expect(selectTuningHz(store.getState())).toBe(442);
        expect(store.getState().pipeline.stages[0].settings).toHaveLength(2);
    });
});
