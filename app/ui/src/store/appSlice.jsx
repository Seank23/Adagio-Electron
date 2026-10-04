import { createSlice } from '@reduxjs/toolkit';

const appSlice = createSlice({
    name: 'app',
    initialState: {
        statusMessage: {type: '', message: ''},
        connectionState: 'connecting',
        engineStatus: {state: 'starting', error: ''},
        loadingFile: '',
    },
    reducers: {
        setStatusMessage: (state, action) => {
            state.statusMessage = action.payload;
        },
        resetStatus: (state) => {
            state.statusMessage = {type: '', message: ''};
        },
        setConnectionState: (state, action) => {
            state.connectionState = action.payload;
        },
        setEngineStatus: (state, action) => {
            state.engineStatus = action.payload;
        },
        setLoadingFile: (state, action) => {
            state.loadingFile = action.payload;
        },
    },
});

export const {
    setStatusMessage,
    resetStatus,
    setConnectionState,
    setEngineStatus,
    setLoadingFile,
} = appSlice.actions;
export default appSlice.reducer;