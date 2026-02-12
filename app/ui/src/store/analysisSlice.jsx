import { createSlice } from '@reduxjs/toolkit';

const analysisSlice = createSlice({
    name: 'analysis',
    initialState: {
        spectrumData: [],
        spectrumSR: 0,
        maxSpectrumValue: 0,
        executionTime: 0,
    },
    reducers: {
        setAnalysisData: (state, action) => {
            state.spectrumData = action.payload?.magnitudes;
            state.spectrumSR = action.payload?.sampleRate;
            state.maxSpectrumValue = action.payload?.maxMagnitude;
            state.executionTime = action.payload?.executionTimeMs;
        },
    },
});

export const { setAnalysisData } = analysisSlice.actions;
export default analysisSlice.reducer;