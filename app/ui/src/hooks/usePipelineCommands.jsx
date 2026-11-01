import { useCallback } from 'react';
import { useDispatch, useStore } from 'react-redux';
import { useEngineCommands } from './useEngineCommands';
import { useReport } from './useReport';
import { setSchema } from '../store/pipelineSlice';
import { selectIsPaused } from '../store/playbackSlice';

export const usePipelineCommands = () => {
    const dispatch = useDispatch();
    const store = useStore();
    const commands = useEngineCommands();
    const report = useReport();

    const apply = useCallback(async request => {
        const result = await request();
        if (result.ok) {
            dispatch(setSchema(result.value));
            if (selectIsPaused(store.getState()))
                commands.analyseFrame().then(report);
        }
        return report(result);
    }, [commands, dispatch, report, store]);

    const switchTo = useCallback(id => apply(() => commands.switchPipeline(id)), [apply, commands]);
    const restoreDefaults = useCallback(() => apply(() => commands.restorePipelineDefaults()), [apply, commands]);

    return { switchTo, restoreDefaults };
};
