import { useCallback, useEffect, useRef } from 'react';
import { useStore } from 'react-redux';
import { selectIsFileOpen, selectIsPlaying } from '../store/playbackSlice';
import { useEngineCommands } from './useEngineCommands';
import { useReport } from './useReport';

const HOLD_DELAY_MS = 400;
const MIN_INTERVAL_MS = 33;

// One gate for the button and the '.' key, so holding both at once still leaves one
// step in flight, and the next is sent no sooner than MIN_INTERVAL_MS after the last.
let inFlight = false;
let lastSentAt = -Infinity;

const msUntilNextStep = () => Math.max(0, MIN_INTERVAL_MS - (performance.now() - lastSentAt));

export const selectCanStep = state => selectIsFileOpen(state) && !selectIsPlaying(state);

export const useFrameStep = () => {
    const store = useStore();
    const commands = useEngineCommands();
    const report = useReport();

    return useCallback(async () => {
        if (inFlight || msUntilNextStep() > 0 || !selectCanStep(store.getState()))
            return null;

        inFlight = true;
        lastSentAt = performance.now();
        try {
            return report(await commands.stepFrame());
        } finally {
            inFlight = false;
        }
    }, [store, commands, report]);
};

const wait = ms => new Promise(resolve => setTimeout(resolve, ms));

// Press-and-hold stepping for a button: one step on press, then repeating after
// HOLD_DELAY_MS until the pointer is released or leaves, a step fails (the end of
// the track), or the transport leaves Ready/Paused. Answers the button's handlers.
export const useStepRepeat = () => {
    const store = useStore();
    const step = useFrameStep();
    const session = useRef(0);

    const stop = useCallback(() => {
        session.current++;
    }, []);

    useEffect(() => stop, [stop]);

    const onPointerDown = useCallback(async event => {
        if (event.button !== 0)
            return;
        const current = ++session.current;
        const held = () => session.current === current;

        const first = await step();
        if (first && !first.ok)
            return;
        await wait(HOLD_DELAY_MS);

        while (held() && selectCanStep(store.getState())) {
            const result = await step();
            if (result && !result.ok)
                return;
            await wait(msUntilNextStep());
        }
    }, [store, step]);

    const onClick = useCallback(event => {
        if (event.detail === 0)
            step();
    }, [step]);

    return {
        onPointerDown,
        onPointerUp: stop,
        onPointerLeave: stop,
        onPointerCancel: stop,
        onClick,
    };
};
