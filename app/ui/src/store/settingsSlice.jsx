import { createSlice } from '@reduxjs/toolkit';

const settingsSlice = createSlice({
    name: 'settings',
    initialState: {
        showLogScale: true,
        // 'system' follows the OS; 'dark' and 'light' force one.
        themeMode: 'system',
    },
    reducers: {
        setShowLogScale: (state, action) => {
            state.showLogScale = action.payload;
        },
        setThemeMode: (state, action) => {
            state.themeMode = action.payload;
        },
    },
});

export const { setShowLogScale, setThemeMode } = settingsSlice.actions;
export default settingsSlice.reducer;
