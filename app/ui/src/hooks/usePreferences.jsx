import { useCallback, useEffect } from 'react';
import { useDispatch, useStore } from 'react-redux';
import { setThemeMode } from '../store/settingsSlice';
import { selectIsPaused } from '../store/playbackSlice';
import { useEngineCommands } from './useEngineCommands';
import { useReport } from './useReport';
import { loadPreferences, savePreferences } from '../utils/preferences';
import { LIMITS } from '../utils/protocol';

// Mounted once, in App: the theme mirrors what main has saved and already applied to the window.
export const usePreferences = () => {
    const dispatch = useDispatch();

    useEffect(() => {
        let cancelled = false;
        loadPreferences().then(result => {
            if (!cancelled && result?.ok)
                dispatch(setThemeMode(result.value.theme));
        });
        return () => {
            cancelled = true;
        };
    }, [dispatch]);
};

// Each preference asks its owner and then shows the owner's answer, as the tuning control does.
// They answer the result, so a caller can wait for it.
export const usePreferenceActions = () => {
    const dispatch = useDispatch();
    const store = useStore();
    const commands = useEngineCommands();
    const report = useReport();

    const setTheme = useCallback(async mode => {
        const result = await savePreferences({ theme: mode });
        if (result.ok)
            dispatch(setThemeMode(result.value.theme));
        else if (result.unsaved)
            dispatch(setThemeMode(mode));
        else
            report(result);
        return result;
    }, [dispatch, report]);

    const setEngineParams = useCallback(async params => {
        const result = await commands.setEngineParams(params);
        if (!result.ok) {
            report(result);
            return result;
        }
        const saved = await savePreferences({ engine: result.value });
        if (!saved.ok && !saved.unsaved)
            report(saved);
        if (selectIsPaused(store.getState()))
            commands.analyseFrame().then(report);
        return result;
    }, [commands, report, store]);

    const restoreDefaults = useCallback(() => Promise.all([
        setTheme('system'),
        setEngineParams({
            sampleRate: LIMITS.sampleRateDefault,
            frameLength: LIMITS.frameLengthDefault,
            hopSize: LIMITS.hopSizeDefault,
            frameSmoothing: LIMITS.frameSmoothingDefault,
        }),
    ]), [setTheme, setEngineParams]);

    return { setTheme, setEngineParams, restoreDefaults };
};
