import { createSlice } from '@reduxjs/toolkit';

const analysisSlice = createSlice({
    name: 'analysis',
    initialState: {
        spectrumData: [],
        notes: [],
        spectrumSR: 0,
        maxSpectrumValue: 0,
        executionTime: 0,
        keyHistogram: [],
        chordHistogram: [],
        detectedKey: null,
        predictedChords: [],
    },
    reducers: {
        setAnalysisData: (state, action) => {
            state.spectrumData = action.payload?.magnitudes;
            state.notes = action.payload?.notes;
            state.spectrumSR = action.payload?.sampleRate;
            state.maxSpectrumValue = action.payload?.maxMagnitude;
            state.executionTime = action.payload?.executionTimeMs;
            state.keyHistogram = action.payload?.keyHistogram || [];
            state.chordHistogram = action.payload?.chordHistogram || [];
            state.detectedKey = action.payload?.detectedKey;
            state.predictedChords = action.payload?.predictedChords || [];
        },
    },
});

export const { setAnalysisData } = analysisSlice.actions;
export default analysisSlice.reducer;