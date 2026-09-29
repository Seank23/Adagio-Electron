import { createSlice } from '@reduxjs/toolkit';
import { TRANSPORT_STATE } from '../utils/protocol';

const playbackSlice = createSlice({
    name: 'playback',
    initialState: {
        currentTime: 0.0,
        duration: 0.0,
        state: TRANSPORT_STATE.EMPTY,
        speed: 1.0,
        volume: 1.0,
    },
    reducers: {
        setCurrentTime(state, action) {
            state.currentTime = action.payload;
        },
        setDuration(state, action) {
            state.duration = action.payload;
        },
        setTransport(state, action) {
            const transport = action.payload ?? {};
            state.state = transport.state ?? state.state;
            state.speed = transport.speed ?? state.speed;
            state.volume = transport.volume ?? state.volume;
            state.duration = transport.duration ?? state.duration;
        },
        resetPlayback(state) {
            state.currentTime = 0.0;
        },
    },
});

export const { setCurrentTime, setDuration, setTransport, resetPlayback } = playbackSlice.actions;

// Derived from the one transport state rather than stored alongside it, so they
// cannot drift apart from it.
export const selectIsPlaying = state => state.playback.state === TRANSPORT_STATE.PLAYING;
export const selectIsPaused = state => state.playback.state === TRANSPORT_STATE.PAUSED;
export const selectIsFileOpen = state =>
    state.playback.state !== TRANSPORT_STATE.EMPTY && state.playback.state !== TRANSPORT_STATE.LOADING;

export default playbackSlice.reducer;
