import { useCallback } from 'react';
import { useDispatch, useStore } from 'react-redux';
import { useEngineCommands } from './useEngineCommands';
import { useReport } from './useReport';
import { setSettingValue } from '../store/pipelineSlice';
import { selectIsPaused } from '../store/playbackSlice';

// Asks the engine for one stage setting and shows its answer, as the tuning control does: the value
// changes only on an ok reply. It answers the result unreported, so the caller reports it once.
export const useSettingCommit = () => {
    const dispatch = useDispatch();
    const store = useStore();
    const commands = useEngineCommands();
    const report = useReport();

    return useCallback(async (stage, key, value) => {
        const result = await commands.setAnalysisSetting(stage, key, value);
        if (result.ok) {
            dispatch(setSettingValue({ stage, key, value }));
            if (selectIsPaused(store.getState()))
                commands.analyseFrame().then(report);
        }
        return result;
    }, [commands, dispatch, report, store]);
};
