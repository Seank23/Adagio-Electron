import { useContext, useMemo } from 'react';
import { WebSocketContext } from '../engine-client/WebSocketContext';
import { createEngineCommands } from '../engine-client/EngineCommands';

// The only way a component talks to the engine. It is stable for the life of the
// provider, so it can sit in an effect's dependency list without re-running it.
export const useEngineCommands = () => {
    const engine = useContext(WebSocketContext);
    return useMemo(() => createEngineCommands(engine), [engine]);
};
