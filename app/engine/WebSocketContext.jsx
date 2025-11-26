import { createContext, useContext, useEffect, useState } from "react";
import { WebSocketEngine } from "./WebSocketEngine";

export const WebSocketContext = createContext(null);

export function WebSocketProvider({ children }) {
    const [socket, setSocket] = useState(null);

    useEffect(() => {
        const wsEngine = new WebSocketEngine('ws://localhost:9001');
        setSocket(wsEngine);
    }, []);

    return (
        <WebSocketContext.Provider value={socket}>
            {children}
        </WebSocketContext.Provider>
    );
}
