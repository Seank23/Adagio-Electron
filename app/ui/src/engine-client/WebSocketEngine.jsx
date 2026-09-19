export const ENGINE_WS_URL = 'ws://127.0.0.1:9001';

export const CONNECTION_STATE = {
    CONNECTING: 'connecting',
    CONNECTED: 'connected',
    DISCONNECTED: 'disconnected',
};

const RECONNECT_BASE_MS = 500;
const RECONNECT_MAX_MS = 10000;
const RECONNECT_JITTER_MS = 250;

export class WebSocketEngine {
    // Constructed idle so that a caller can subscribe before the first connection
    // attempt, and so that open() and close() can be paired with a mount.
    constructor(url = ENGINE_WS_URL) {
        this.url = url;
        this.ws = null;
        this.listeners = [];
        this.stateListeners = [];
        this.state = CONNECTION_STATE.DISCONNECTED;
        this.attempt = 0;
        this.reconnectTimer = null;
        this.closed = true;
    }

    open() {
        if (!this.closed) return;
        this.closed = false;
        this.attempt = 0;
        this.connect();
    }

    connect() {
        if (this.closed) return;

        this.setState(CONNECTION_STATE.CONNECTING);
        const ws = new WebSocket(this.url);
        this.ws = ws;

        ws.onopen = () => {
            this.attempt = 0;
            this.setState(CONNECTION_STATE.CONNECTED);
        };

        ws.onmessage = event => {
            let msg = null;
            try {
                msg = JSON.parse(event.data);
            } catch {
                console.error('Invalid message from backend:', event.data);
                return;
            }
            this.listeners.forEach(callback => callback(msg));
        };

        ws.onerror = () => {};

        ws.onclose = () => {
            if (this.ws !== ws) return;
            this.ws = null;
            this.setState(CONNECTION_STATE.DISCONNECTED);
            this.scheduleReconnect();
        };
    }

    scheduleReconnect() {
        if (this.closed || this.reconnectTimer) return;

        const delay = Math.min(RECONNECT_BASE_MS * 2 ** this.attempt, RECONNECT_MAX_MS);
        this.attempt += 1;
        this.reconnectTimer = setTimeout(() => {
            this.reconnectTimer = null;
            this.connect();
        }, delay + Math.random() * RECONNECT_JITTER_MS);
    }

    setState(state) {
        if (this.state === state) return;
        this.state = state;
        this.stateListeners.forEach(callback => callback(state));
    }

    addListener(callback) {
        this.listeners.push(callback);
    }

    removeListener(callback) {
        this.listeners = this.listeners.filter(cb => cb !== callback);
    }

    addStateListener(callback) {
        this.stateListeners.push(callback);
    }

    removeStateListener(callback) {
        this.stateListeners = this.stateListeners.filter(cb => cb !== callback);
    }

    send(data) {
        if (this.ws?.readyState === WebSocket.OPEN)
            this.ws.send(JSON.stringify(data));
    }

    // Leaves the listeners in place: the same engine can be reopened, and the
    // subscribers outlive a StrictMode remount.
    close() {
        if (this.closed) return;
        this.closed = true;
        clearTimeout(this.reconnectTimer);
        this.reconnectTimer = null;
        const ws = this.ws;
        this.ws = null;
        ws?.close();
        this.setState(CONNECTION_STATE.DISCONNECTED);
    }
}
