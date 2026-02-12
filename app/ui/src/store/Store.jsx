import { configureStore } from '@reduxjs/toolkit';
import appReducer from './appSlice';
import playbackReducer from './playbackSlice';
import analysisReducer from './analysisSlice';
import settingsReducer from './settingsSlice';

export const store = configureStore({
    reducer: {
        app: appReducer,
        playback: playbackReducer,
        analysis: analysisReducer,
        settings: settingsReducer,
    },
});