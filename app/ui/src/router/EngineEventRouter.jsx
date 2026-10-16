import { useEffect } from 'react';
import { useDispatch, useStore } from 'react-redux';
import { useEngineEvents, useEngineFrames, useEngineConnection } from '../hooks/useEngineEvents';
import { useEngineCommands } from '../hooks/useEngineCommands';
import { CONNECTION_STATE } from '../engine-client/WebSocketEngine';
import { publishSpectrum, resetSpectrum, stageWaveform, commitWaveform, clearFrames } from '../engine-client/FrameStore';
import { BINARY_FRAME, TRANSPORT_STATE } from '../utils/protocol';
import { engineParamsToRestore, loadPreferences } from '../utils/preferences';
import { setDuration, setCurrentTime, setTransport, resetPlayback, selectIsPaused } from '../store/playbackSlice';
import { setStatusMessage, setConnectionState, setEngineStatus } from '../store/appSlice';
import { EVENT_TYPE } from '../utils/utils';
import { setAnalysisData, resetAnalysis } from '../store/analysisSlice';
import { setSchema } from '../store/pipelineSlice';

export default function EngineEventRouter() {
    const dispatch = useDispatch();
    const store = useStore();
    const connectionState = useEngineConnection();
    const commands = useEngineCommands();

    useEffect(() => {
        dispatch(setConnectionState(connectionState));
    }, [connectionState, dispatch]);

    // Transport events describe changes, so a client that arrives mid-session has to
    // ask once. This is what makes a reload, or a reconnect to an engine that is
    // already playing, show the truth instead of an empty transport. The waveform went
    // out once with the load, so with a file open it is asked for too: its frames are
    // staged as they arrive and published on the reply, which comes after them.
    useEffect(() => {
        if (connectionState !== CONNECTION_STATE.CONNECTED)
            return;

        let cancelled = false;

        const restoreEngineParams = async transport => {
            if (transport?.state === TRANSPORT_STATE.LOADING)
                return;
            const saved = await loadPreferences();
            const changes = saved?.ok ? engineParamsToRestore(saved.value.engine, transport?.analysis) : null;
            if (cancelled || !changes)
                return;
            const result = await commands.setEngineParams(changes);
            if (!result.ok) {
                dispatch(setStatusMessage({ type: 'error', message: `Couldn't restore the saved analysis settings: ${result.error}` }));
                return;
            }
            if (selectIsPaused(store.getState()))
                commands.analyseFrame();
        };

        commands.status().then(result => {
            if (cancelled || !result.ok)
                return;
            dispatch(setTransport(result.value));
            if (result.value?.track) {
                commands.getWaveform().then(reply => {
                    if (reply.ok)
                        commitWaveform();
                });
            }
            restoreEngineParams(result.value);
        });
        commands.getAnalysisSchema().then(result => {
            if (!cancelled && result.ok)
                dispatch(setSchema(result.value));
        });
        return () => {
            cancelled = true;
        };
    }, [connectionState, commands, dispatch, store]);

    // The other half of the engine's health: whether main could start the process at
    // all. The socket only says whether it is reachable now. Outside Electron there is
    // no main, and so no status to follow.
    useEffect(() => {
        return window.api?.onEngineStatus?.(status => dispatch(setEngineStatus(status)));
    }, [dispatch]);

    useEngineEvents(async msg => {
        switch (msg.type) {
        case EVENT_TYPE.TRANSPORT:
            dispatch(setTransport(msg?.value));
            break;
        case EVENT_TYPE.FILE_LOADED: {
            commitWaveform();
            resetSpectrum();
            dispatch(setDuration(msg?.value?.duration));
            const schema = await commands.getAnalysisSchema();
            if (schema.ok)
                dispatch(setSchema(schema.value));
            break;
        }
        case EVENT_TYPE.FILE_CLOSED:
            clearFrames();
            dispatch(setStatusMessage({ type: 'info', message: 'Audio file closed' }));
            dispatch(resetPlayback());
            dispatch(setDuration(0));
            break;
        case EVENT_TYPE.POSITION:
            dispatch(setCurrentTime(msg?.value));
            break;
        case EVENT_TYPE.END_OF_PLAY:
            dispatch(resetPlayback());
            if (store.getState().settings.repeat) {
                const result = await commands.play();
                if (!result.ok)
                    dispatch(setStatusMessage({ type: 'error', message: result.error }));
            }
            break;
        case EVENT_TYPE.ERROR:
            dispatch(setStatusMessage({ type: 'error', message: msg?.value }));
            break;
        case EVENT_TYPE.INFO:
            dispatch(setStatusMessage({ type: 'info', message: msg?.value }));
            break;
        case EVENT_TYPE.ANALYSIS:
            dispatch(setAnalysisData(msg?.value));
            break;
        case EVENT_TYPE.ANALYSIS_RESET:
            resetSpectrum();
            dispatch(resetAnalysis());
            break;
        default:
            break;
        }
    });

    useEngineFrames(frame => {
        switch (frame.kind) {
        case BINARY_FRAME.KIND.SPECTRUM:
            publishSpectrum(frame);
            break;
        case BINARY_FRAME.KIND.WAVEFORM:
            stageWaveform(frame);
            break;
        default:
            break;
        }
    });

    return null;
}
