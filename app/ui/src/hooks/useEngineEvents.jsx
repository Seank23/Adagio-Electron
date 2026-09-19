import { useCallback, useContext, useEffect, useRef, useSyncExternalStore } from 'react';
import { WebSocketContext } from '../engine-client/WebSocketContext';
import { CONNECTION_STATE } from '../engine-client/WebSocketEngine';

export const useEngineEvents = callback => {
    const ws = useContext(WebSocketContext);
    const callbackRef = useRef(callback);

    useEffect(() => {
        callbackRef.current = callback;
    });

    useEffect(() => {
        if (!ws) return;

        const listener = msg => callbackRef.current(msg);
        ws.addListener(listener);
        return () => ws.removeListener(listener);
    }, [ws]);
};

export const useEngineConnection = () => {
    const ws = useContext(WebSocketContext);

    const subscribe = useCallback(onChange => {
        if (!ws) return () => {};
        ws.addStateListener(onChange);
        return () => ws.removeStateListener(onChange);
    }, [ws]);

    const getSnapshot = useCallback(() => ws?.state ?? CONNECTION_STATE.CONNECTING, [ws]);

    return useSyncExternalStore(subscribe, getSnapshot);
};
