import { createSlice } from '@reduxjs/toolkit';

const appSlice = createSlice({
    name: 'app',
    initialState: {
        statusMessage: {type: '', message: ''},
        isFileOpen: false,
        canvasWidth: 1000,
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
    },
});

export const { setStatusMessage, resetStatus, setIsFileOpen, setCanvasWidth } = appSlice.actions;
export default appSlice.reducer;