import { createSlice } from '@reduxjs/toolkit';
import { A4_DEFAULT_HZ } from '../utils/music';

export const TUNING_STAGE = 'NoteDetector';
export const TUNING_KEY = 'A440_TUNING';

const findSetting = (stages, stage, key) =>
    stages.find(s => s.name === stage)?.settings.find(s => s.key === key);

const pipelineSlice = createSlice({
    name: 'pipeline',
    initialState: {
        stages: [],
    },
    reducers: {
        setSchema(state, action) {
            state.stages = action.payload?.stages ?? [];
        },
        setSettingValue(state, action) {
            const { stage, key, value } = action.payload;
            const setting = findSetting(state.stages, stage, key);
            if (setting)
                setting.value = value;
        },
    },
});

export const { setSchema, setSettingValue } = pipelineSlice.actions;

const selectTuningSetting = state => findSetting(state.pipeline.stages, TUNING_STAGE, TUNING_KEY);

// Numbers, so a component selecting them re-renders only when they change.
export const selectTuningHz = state => {
    const value = selectTuningSetting(state)?.value;
    return typeof value === 'number' ? value : A4_DEFAULT_HZ;
};
export const selectTuningMin = state => selectTuningSetting(state)?.min ?? 415;
export const selectTuningMax = state => selectTuningSetting(state)?.max ?? 466;
export const selectHasSchema = state => state.pipeline.stages.length > 0;

export default pipelineSlice.reducer;
