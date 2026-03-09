import { createSlice } from '@reduxjs/toolkit';

const analysisSlice = createSlice({
    name: 'analysis',
    initialState: {
        spectrumData: [],
        notes: [],
        spectrumSR: 0,
        maxSpectrumValue: 0,
        executionTime: 0,
        frequencyHistogram: [],
        detectedKey: null,
    },
    reducers: {
        setAnalysisData: (state, action) => {
            state.spectrumData = action.payload?.magnitudes;
            state.notes = action.payload?.notes;
            state.spectrumSR = action.payload?.sampleRate;
            state.maxSpectrumValue = action.payload?.maxMagnitude;
            state.executionTime = action.payload?.executionTimeMs;
            state.frequencyHistogram = action.payload?.frequencyHistogram || [];
            state.detectedKey = action.payload?.detectedKey;
        },
    },
});

export const { setAnalysisData } = analysisSlice.actions;
export default analysisSlice.reducer;