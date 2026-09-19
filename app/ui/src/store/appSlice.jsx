import { createSlice } from '@reduxjs/toolkit';

const appSlice = createSlice({
    name: 'app',
    initialState: {
        statusMessage: {type: '', message: ''},
        isFileOpen: false,
        canvasWidth: 1000,
        connectionState: 'connecting',
        engineStatus: {state: 'starting', error: ''},
    },
    reducers: {
        setStatusMessage: (state, action) => {
            state.statusMessage = action.payload;
        },
        resetStatus: (state) => {
            state.statusMessage = {type: '', message: ''};
        },
        setIsFileOpen: (state, action) => {
            state.isFileOpen = action.payload;
        },
        setCanvasWidth: (state, action) => {
            state.canvasWidth = action.payload;
        },
        setConnectionState: (state, action) => {
            state.connectionState = action.payload;
        },
        setEngineStatus: (state, action) => {
            state.engineStatus = action.payload;
        },
    },
});

export const {
    setStatusMessage,
    resetStatus,
    setIsFileOpen,
    setCanvasWidth,
    setConnectionState,
    setEngineStatus,
} = appSlice.actions;
export default appSlice.reducer;