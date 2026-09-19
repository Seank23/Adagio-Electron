import { useEffect, useState } from 'react';
import { WebSocketContext } from './WebSocketContext';
import { WebSocketEngine } from './WebSocketEngine';

export const WebSocketProvider = ({ children }) => {
    // One engine for the life of the provider, so the context value never changes and
    // subscribers are not re-registered on every render.
    const [engine] = useState(() => new WebSocketEngine());

    useEffect(() => {
        engine.open();
        // Closing on unmount matters under StrictMode, which mounts twice: without it
        // the first socket stays open and the engine keeps broadcasting to it.
        return () => engine.close();
    }, [engine]);

    return (
        <WebSocketContext.Provider value={engine}>
            {children}
        </WebSocketContext.Provider>
    );
};
