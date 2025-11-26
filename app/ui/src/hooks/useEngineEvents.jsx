import { useEffect, useContext } from 'react'
import { WebSocketContext } from '../../../engine/WebSocketContext'

export function useEngineEvents(callback) {
    const ws = useContext(WebSocketContext);

    useEffect(() => {
        if (!ws) return;
        ws.addListener(msg => callback(msg));
        return () => ws.removeListener(callback);
    }, [ws]);
}