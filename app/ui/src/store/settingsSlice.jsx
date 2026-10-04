import { createSlice } from '@reduxjs/toolkit';

const settingsSlice = createSlice({
    name: 'settings',
    initialState: {
        showLogScale: true,
        // 'system' follows the OS; 'dark' and 'light' force one.
        themeMode: 'system',
        repeat: false,
        follow: true,
        sidebarTab: 'analysis',
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
        setSidebarTab: (state, action) => {
            state.sidebarTab = action.payload;
        },
    },
});

export const { setShowLogScale, setThemeMode, toggleRepeat, toggleFollow, setSidebarTab } = settingsSlice.actions;
export default settingsSlice.reducer;
