import { configureStore } from '@reduxjs/toolkit';
import playbackReducer from './PlaybackSlice';

export const store = configureStore({
    reducer: {
        playback: playbackReducer,
    },
});