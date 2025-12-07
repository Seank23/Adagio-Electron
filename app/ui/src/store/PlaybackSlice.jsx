import { createSlice } from '@reduxjs/toolkit';

const playbackSlice = createSlice({
    name: 'playback',
    initialState: {
        currentSample: 0,
        totalSamples: 0,
        isStarted: false,
        isPlaying: false,
    },
    reducers: {
        setCurrentSample(state, action) {
            state.currentSample = action.payload;
        },
        setTotalSamples(state, action) {
            state.totalSamples = action.payload;
        },
        setIsStarted(state, action) {
            state.isStarted = action.payload;
        },
        setIsPlaying(state, action) {
            state.isPlaying = action.payload;
        },
        resetPlayback(state) {
            state.currentSample = 0;
            state.isStarted = false;
            state.isPlaying = false;
        },
    },
});

export const { setCurrentSample, setTotalSamples, setIsPlaying, setIsStarted, resetPlayback } = playbackSlice.actions;
export default playbackSlice.reducer;