import { createSlice } from '@reduxjs/toolkit';

const playbackSlice = createSlice({
    name: 'playback',
    initialState: {
        currentTime: 0.0,
        duration: 0.0,
        isStarted: false,
        isPlaying: false,
        waveformData: null,
    },
    reducers: {
        setCurrentTime(state, action) {
            state.currentTime = action.payload;
        },
        setDuration(state, action) {
            state.duration = action.payload;
        },
        setIsStarted(state, action) {
            state.isStarted = action.payload;
        },
        setIsPlaying(state, action) {
            state.isPlaying = action.payload;
        },
        setWaveformData(state, action) {
            state.waveformData = action.payload;
        },
        resetPlayback(state) {
            state.currentTime = 0.0;
            state.isStarted = false;
            state.isPlaying = false;
            state.waveformData = null;
        },
    },
});

export const { setCurrentTime, setDuration, setIsPlaying, setIsStarted, setWaveformData, resetPlayback } = playbackSlice.actions;
export default playbackSlice.reducer;