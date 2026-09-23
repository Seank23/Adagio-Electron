import { useEffect } from 'react';
import { useDispatch } from 'react-redux';
import { useEngineEvents, useEngineConnection } from '../hooks/useEngineEvents';
import { useEngineCommands } from '../hooks/useEngineCommands';
import { CONNECTION_STATE } from '../engine-client/WebSocketEngine';
import { setDuration, setCurrentTime, setWaveformData, setTransport, resetPlayback } from '../store/playbackSlice';
import { setStatusMessage, setConnectionState, setEngineStatus } from '../store/appSlice';
import { EVENT_TYPE } from '../utils/utils';
import { setAnalysisData } from '../store/analysisSlice';

export default function EngineEventRouter() {
    const dispatch = useDispatch();
    const connectionState = useEngineConnection();
    const commands = useEngineCommands();

    useEffect(() => {
        dispatch(setConnectionState(connectionState));
    }, [connectionState, dispatch]);

    // Transport events describe changes, so a client that arrives mid-session has to
    // ask once. This is what makes a reload, or a reconnect to an engine that is
    // already playing, show the truth instead of an empty transport.
    useEffect(() => {
        if (connectionState !== CONNECTION_STATE.CONNECTED)
            return;

        let cancelled = false;
        commands.status().then(result => {
            if (!cancelled && result.ok)
                dispatch(setTransport(result.value));
        });
        return () => {
            cancelled = true;
        };
    }, [connectionState, commands, dispatch]);

    // The other half of the engine's health: whether main could start the process at
    // all. The socket only says whether it is reachable now.
    useEffect(() => {
        return window.api.onEngineStatus(status => dispatch(setEngineStatus(status)));
    }, [dispatch]);

    useEngineEvents(async msg => {
        switch (msg.type) {
        case EVENT_TYPE.TRANSPORT:
            dispatch(setTransport(msg?.value));
            break;
        case EVENT_TYPE.FILE_LOADED:
            dispatch(setDuration(msg?.value?.duration));
            break;
        case EVENT_TYPE.FILE_CLOSED:
            dispatch(setStatusMessage({ type: 'info', message: 'Audio file closed' }));
            dispatch(resetPlayback());
            dispatch(setDuration(0));
            break;
        case EVENT_TYPE.POSITION:
            dispatch(setCurrentTime(msg?.value));
            break;
        case EVENT_TYPE.END_OF_PLAY:
            await commands.stop();
            dispatch(resetPlayback());
            break;
        case EVENT_TYPE.ERROR:
            dispatch(setStatusMessage({ type: 'error', message: msg?.value }));
            break;
        case EVENT_TYPE.INFO:
            dispatch(setStatusMessage({ type: 'info', message: msg?.value }));
            break;
        case EVENT_TYPE.WAVEFORM_DATA:
            dispatch(setWaveformData(msg?.value));
            break;
        case EVENT_TYPE.ANALYSIS:
            dispatch(setAnalysisData(msg?.value));
            break;
        default:
            break;
        }
    });

    return null;
}
