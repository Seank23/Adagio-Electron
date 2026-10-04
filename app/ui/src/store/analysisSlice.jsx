import { createSelector, createSlice } from '@reduxjs/toolkit';
import { midiOf } from '../utils/music';
import { selectTuningHz } from './pipelineSlice';

const initialState = {
    notes: [],
    spectrumSR: 0,
    executionTime: 0,
    keyHistogram: [],
    chordHistogram: [],
    detectedKey: null,
    predictedChords: [],
};

const analysisSlice = createSlice({
    name: 'analysis',
    initialState,
    reducers: {
        setAnalysisData: (state, action) => {
            state.notes = action.payload?.notes;
            state.spectrumSR = action.payload?.sampleRate;
            state.executionTime = action.payload?.executionTimeMs;
            state.keyHistogram = action.payload?.keyHistogram || [];
            state.chordHistogram = action.payload?.chordHistogram || [];
            const key = action.payload?.detectedKey;
            if (!key?.name)
                state.detectedKey = null;
            else if (key.name !== state.detectedKey?.name)
                state.detectedKey = key;
            state.predictedChords = action.payload?.predictedChords || [];
        },
        resetAnalysis: state => ({ ...initialState, spectrumSR: state.spectrumSR }),
    },
});

export const { setAnalysisData, resetAnalysis } = analysisSlice.actions;

export const CHORD_ROWS = 4;

export const selectAnalysisRate = state =>
    state.analysis.spectrumSR || state.playback.track?.analysisSampleRate || 0;

export const selectPitchClassPercents = createSelector(
    [state => state.analysis.keyHistogram, selectTuningHz],
    (histogram, a4) => {
        const bins = new Array(12).fill(0);
        histogram.forEach(({ frequency, score }) => {
            if (frequency > 0)
                bins[((midiOf(frequency, a4) % 12) + 12) % 12] += score;
        });
        const total = bins.reduce((sum, value) => sum + value, 0);
        return total > 0 ? bins.map(value => (value * 100) / total) : bins;
    },
);

export const selectChordProbabilities = createSelector(
    [state => state.analysis.predictedChords],
    chords => chords.slice(0, CHORD_ROWS).map(chord => chord.probability),
);

// A string, so the chord rows re-render only when a name or the count changes.
export const selectChordNamesKey = state =>
    state.analysis.predictedChords.slice(0, CHORD_ROWS).map(chord => chord.name).join('|');

export default analysisSlice.reducer;
