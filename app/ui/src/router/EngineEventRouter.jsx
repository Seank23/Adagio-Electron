import React from 'react';
import { useDispatch } from 'react-redux';
import { useEngineEvents } from '../hooks/useEngineEvents';
import { setTotalSamples, setCurrentSample, resetPlayback } from '../store/PlaybackSlice';
import { EVENT_TYPE } from '../utils/utils';

export default function EngineEventRouter() {
    const dispatch = useDispatch();

    useEngineEvents(async msg => {
        switch (msg.type) {
        case EVENT_TYPE.FILE_LOADED:
            dispatch(setTotalSamples(msg?.value?.totalSamples));
            break;
        case EVENT_TYPE.POSITION:
            dispatch(setCurrentSample(msg?.value));
            break;
        case EVENT_TYPE.END_OF_PLAY:
            await window.api.stop();
            dispatch(resetPlayback());
            break;
        default:
            break;
        }
    });

    return null;
}