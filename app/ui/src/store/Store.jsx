import { configureStore } from '@reduxjs/toolkit';
import playbackReducer from './PlaybackSlice';
import appReducer from './appSlice';

export const store = configureStore({
    reducer: {
        playback: playbackReducer,
        app: appReducer,
    },
});