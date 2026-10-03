import { useRef, useState } from 'react';
import { useReport } from './useReport';

const SLIDER_SEND_INTERVAL_MS = 50;

const toPercent = value => Math.round(value * 100);

// A slider over an engine value. While a handle is held the user owns the value and
// sends it at most every 50 ms; otherwise it shows the engine's.
export const useEngineSlider = (engineValue, send, sendWhileDragging = true) => {
    const report = useReport();
    const [dragPercent, setDragPercent] = useState(null);
    const lastSendRef = useRef(0);
    const releaseRef = useRef(0);

    const onChange = percent => {
        releaseRef.current += 1;
        setDragPercent(percent);
        const now = Date.now();
        if (now - lastSendRef.current < SLIDER_SEND_INTERVAL_MS)
            return;
        lastSendRef.current = now;
        if (sendWhileDragging) send(percent / 100).then(report);
    };

    const onChangeComplete = async percent => {
        const release = ++releaseRef.current;
        setDragPercent(percent);
        lastSendRef.current = 0;
        report(await send(percent / 100));
        // A drag that started while this was in flight keeps the value.
        if (release === releaseRef.current)
            setDragPercent(null);
    };

    return {
        percent: dragPercent ?? toPercent(engineValue),
        onChange,
        onChangeComplete,
    };
};
