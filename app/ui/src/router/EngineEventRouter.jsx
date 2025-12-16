import { useDispatch } from 'react-redux';
import { useEngineEvents } from '../hooks/useEngineEvents';
import { setTotalSamples, setCurrentSample, resetPlayback } from '../store/PlaybackSlice';
import { setStatusMessage, setIsFileOpen } from '../store/appSlice'; 
import { EVENT_TYPE } from '../utils/utils';

export default function EngineEventRouter() {
    const dispatch = useDispatch();

    useEngineEvents(async msg => {
        switch (msg.type) {
        case EVENT_TYPE.FILE_LOADED:
            dispatch(setIsFileOpen(true));
            dispatch(setTotalSamples(msg?.value?.totalSamples));
            break;
        case EVENT_TYPE.FILE_CLOSED:
            dispatch(setIsFileOpen(false));
            dispatch(setStatusMessage({ type: 'info', message: 'Audio file closed' }));
            dispatch(resetPlayback());
            dispatch(setTotalSamples(0));
            break;
        case EVENT_TYPE.POSITION:
            dispatch(setCurrentSample(msg?.value));
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
        default:
            break;
        }
    });

    return null;
}