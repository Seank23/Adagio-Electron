import { useEffect, useRef } from 'react';
import { useStoreListener } from './useStoreListener';

// useStoreListener, coalesced to one paint per animation frame. For DOM written through
// refs: a value that changes at 31 Hz is painted at most once a frame, without a render.
// The returned ref holds the latest value, for a repaint after the elements change.
export const useStoreFrameListener = (selector, paint) => {
    const valueRef = useRef(undefined);
    const frameRef = useRef(0);
    const paintRef = useRef(paint);

    useEffect(() => {
        paintRef.current = paint;
    });

    useStoreListener(selector, value => {
        valueRef.current = value;
        if (frameRef.current) return;
        frameRef.current = requestAnimationFrame(() => {
            frameRef.current = 0;
            paintRef.current(valueRef.current);
        });
    });

    useEffect(() => () => {
        cancelAnimationFrame(frameRef.current);
        frameRef.current = 0;
    }, []);

    return valueRef;
};
