import { createSlice } from '@reduxjs/toolkit';

const analysisSlice = createSlice({
    name: 'analysis',
    initialState: {
        spectrumData: [],
        spectrumPeaks: [],
        spectrumSR: 0,
        maxSpectrumValue: 0,
        executionTime: 0,
        localMedianData: [],
    },
    reducers: {
        setAnalysisData: (state, action) => {
            state.spectrumData = action.payload?.magnitudes;
            state.spectrumPeaks = action.payload?.peaks;
            state.spectrumSR = action.payload?.sampleRate;
            state.maxSpectrumValue = action.payload?.maxMagnitude;
            state.executionTime = action.payload?.executionTimeMs;
            state.localMedianData = action.payload?.localMedian;
        },
    },
});

export const { setAnalysisData } = analysisSlice.actions;
export default analysisSlice.reducer;