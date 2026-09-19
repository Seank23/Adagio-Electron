import { useEffect } from 'react';
import { useDispatch } from 'react-redux';
import { useEngineEvents, useEngineConnection } from '../hooks/useEngineEvents';
import { setDuration, setCurrentTime, setWaveformData, resetPlayback } from '../store/playbackSlice';
import { setStatusMessage, setIsFileOpen, setConnectionState, setEngineStatus } from '../store/appSlice';
import { EVENT_TYPE } from '../utils/utils';
import { setAnalysisData } from '../store/analysisSlice';

export default function EngineEventRouter() {
    const dispatch = useDispatch();
    const connectionState = useEngineConnection();

    useEffect(() => {
        dispatch(setConnectionState(connectionState));
    }, [connectionState, dispatch]);

    // The other half of the engine's health: whether main could start the process at
    // all. The socket only says whether it is reachable now.
    useEffect(() => {
        return window.api.onEngineStatus(status => dispatch(setEngineStatus(status)));
    }, [dispatch]);

    useEngineEvents(async msg => {
        switch (msg.type) {
        case EVENT_TYPE.FILE_LOADED:
            dispatch(setIsFileOpen(true));
            dispatch(setDuration(msg?.value?.duration));
            break;
        case EVENT_TYPE.FILE_CLOSED:
            dispatch(setIsFileOpen(false));
            dispatch(setStatusMessage({ type: 'info', message: 'Audio file closed' }));
            dispatch(resetPlayback());
            dispatch(setDuration(0));
            break;
        case EVENT_TYPE.POSITION:
            dispatch(setCurrentTime(msg?.value));
            break;
        case EVENT_TYPE.END_OF_PLAY:
            await window.api.stop();
            dispatch(resetPlayback());
            break;
        case EVENT_TYPE.SUCCESS:
            dispatch(setStatusMessage({ type: 'success', message: msg?.value }));
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
