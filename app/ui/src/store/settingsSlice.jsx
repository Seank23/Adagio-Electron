import { createSlice } from '@reduxjs/toolkit';

const settingsSlice = createSlice({
    name: 'settings',
    initialState: {
        showDecibels: false,
        // 'system' follows the OS; 'dark' and 'light' force one.
        themeMode: 'system',
        repeat: false,
        follow: true,
        sidebarTab: 'analysis',
        openStages: [],
    },
    reducers: {
        setShowDecibels: (state, action) => {
            state.showDecibels = action.payload;
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
        toggleStage: (state, action) => {
            const name = action.payload;
            state.openStages = state.openStages.includes(name)
                ? state.openStages.filter(open => open !== name)
                : [...state.openStages, name];
        },
    },
});

export const { setShowDecibels, setThemeMode, toggleRepeat, toggleFollow, setSidebarTab, toggleStage } = settingsSlice.actions;
export default settingsSlice.reducer;
