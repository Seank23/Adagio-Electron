import { createSlice } from '@reduxjs/toolkit';

const settingsSlice = createSlice({
    name: 'settings',
    initialState: {
        showLogScale: true,
        // 'system' follows the OS; 'dark' and 'light' force one.
        themeMode: 'system',
        repeat: false,
        follow: true,
    },
    reducers: {
        setShowLogScale: (state, action) => {
            state.showLogScale = action.payload;
        },
        setThemeMode: (state, action) => {
            state.themeMode = action.payload;
        },
        toggleRepeat: state => {
            state.repeat = !state.repeat;
        },
        toggleFollow: state => {
            state.follow = !state.follow;
        },
    },
});

export const { setShowLogScale, setThemeMode, toggleRepeat, toggleFollow } = settingsSlice.actions;
export default settingsSlice.reducer;
