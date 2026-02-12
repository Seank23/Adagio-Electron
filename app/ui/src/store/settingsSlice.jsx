import { createSlice } from '@reduxjs/toolkit';

const settingsSlice = createSlice({
    name: 'settings',
    initialState: {
        showLogScale: false,
    },
    reducers: {
        setShowLogScale: (state, action) => {
            state.showLogScale = action.payload;
        },
    },
});

export const { setShowLogScale } = settingsSlice.actions;
export default settingsSlice.reducer;