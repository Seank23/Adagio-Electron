import { createSlice } from '@reduxjs/toolkit';

const analysisSlice = createSlice({
    name: 'analysis',
    initialState: {
        spectrumData: [],
        spectrumSR: 0,
        executionTime: 0,
    },
    reducers: {
        setSpectrumData: (state, action) => {
            state.spectrumData = action.payload;
        },
        setSpectrumSR: (state, action) => {
            state.spectrumSR = action.payload;
        },
        setExecutionTime: (state, action) => {
            state.executionTime = action.payload;
        },
    },
});

export const { setSpectrumData, setSpectrumSR, setExecutionTime } = analysisSlice.actions;
export default analysisSlice.reducer;