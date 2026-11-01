import { useRef, useState } from 'react';
import { useReport } from './useReport';

const SLIDER_SEND_INTERVAL_MS = 50;

// Volume and speed travel as 0–1 and show as percent.
const PERCENT = {
    toDisplay: value => Math.round(value * 100),
    fromDisplay: shown => shown / 100,
};

// A slider over an engine value. While a handle is held the user owns the value and
// sends it at most every 50 ms; otherwise it shows the engine's. toDisplay and fromDisplay
// convert between the engine's value and the slider's, percent unless told otherwise.
export const useEngineSlider = (engineValue, send, {
    sendWhileDragging = true,
    toDisplay = PERCENT.toDisplay,
    fromDisplay = PERCENT.fromDisplay,
} = {}) => {
    const report = useReport();
    const [dragValue, setDragValue] = useState(null);
    const lastSendRef = useRef(0);
    const releaseRef = useRef(0);

    const onChange = shown => {
        releaseRef.current += 1;
        setDragValue(shown);
        const now = Date.now();
        if (now - lastSendRef.current < SLIDER_SEND_INTERVAL_MS)
            return;
        lastSendRef.current = now;
        if (sendWhileDragging) send(fromDisplay(shown)).then(report);
    };

    const onChangeComplete = async shown => {
        const release = ++releaseRef.current;
        setDragValue(shown);
        lastSendRef.current = 0;
        report(await send(fromDisplay(shown)));
        // A drag that started while this was in flight keeps the value.
        if (release === releaseRef.current)
            setDragValue(null);
    };

    return {
        value: dragValue ?? toDisplay(engineValue),
        onChange,
        onChangeComplete,
    };
};
