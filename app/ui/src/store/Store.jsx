import { configureStore } from '@reduxjs/toolkit';
import appReducer from './appSlice';
import playbackReducer from './playbackSlice';
import analysisReducer from './analysisSlice';

export const store = configureStore({
    reducer: {
        app: appReducer,
        playback: playbackReducer,
        analysis: analysisReducer,
    },
});