import { createSlice } from '@reduxjs/toolkit';

const analysisSlice = createSlice({
    name: 'analysis',
    initialState: {
        notes: [],
        spectrumSR: 0,
        executionTime: 0,
        keyHistogram: [],
        chordHistogram: [],
        detectedKey: null,
        predictedChords: [],
    },
    reducers: {
        setAnalysisData: (state, action) => {
            state.notes = action.payload?.notes;
            state.spectrumSR = action.payload?.sampleRate;
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