import { createContext } from 'react';

// Kept apart from the provider so that the module exports a component or a value,
// never both, which is what fast refresh needs.
export const WebSocketContext = createContext(null);
