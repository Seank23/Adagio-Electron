import { createSlice } from '@reduxjs/toolkit';

const appSlice = createSlice({
    name: 'app',
    initialState: {
        statusMessage: {type: '', message: ''},
        isFileOpen: false,
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
    },
});

export const { setStatusMessage, resetStatus, setIsFileOpen } = appSlice.actions;
export default appSlice.reducer;