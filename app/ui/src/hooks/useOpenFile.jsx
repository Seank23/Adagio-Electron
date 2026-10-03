import { useCallback } from 'react';
import { useDispatch } from 'react-redux';
import { setLoadingFile, setStatusMessage } from '../store/appSlice';
import { useEngineCommands } from './useEngineCommands';
import { useReport } from './useReport';

// One load path for the dialog and a drop: the status message, the pending file the
// empty state shows, and the engine's answer.
export const useLoadFile = () => {
    const dispatch = useDispatch();
    const commands = useEngineCommands();
    const report = useReport();

    return useCallback(async path => {
        dispatch(setLoadingFile(path));
        dispatch(setStatusMessage({ type: 'loading', message: 'Loading audio...' }));
        const result = report(await commands.load(path));
        dispatch(setLoadingFile(''));
        return result;
    }, [dispatch, commands, report]);
};

export const useOpenFile = () => {
    const dispatch = useDispatch();
    const loadFile = useLoadFile();

    return useCallback(async () => {
        if (!window.api?.selectAudioFile) {
            dispatch(setStatusMessage({ type: 'error', message: 'Opening a file needs the desktop app.' }));
            return;
        }
        const file = await window.api.selectAudioFile();
        if (file)
            await loadFile(file);
    }, [dispatch, loadFile]);
};
