import { ENGINE_WS_URL, EVENT_TYPE, TOKEN_PARAM } from '../utils/protocol';
import { decodeFrame } from './BinaryFrame';

export const CONNECTION_STATE = {
    CONNECTING: 'connecting',
    CONNECTED: 'connected',
    DISCONNECTED: 'disconnected',
};

const RECONNECT_BASE_MS = 500;
const RECONNECT_MAX_MS = 10000;
const RECONNECT_JITTER_MS = 250;
const REQUEST_TIMEOUT_MS = 30000;

export class WebSocketEngine {
    // Constructed idle so that a caller can subscribe before the first connection
    // attempt, and so that open() and close() can be paired with a mount.
    constructor(url = ENGINE_WS_URL) {
        this.url = url;
        this.ws = null;
        this.listeners = [];
        this.frameListeners = [];
        this.stateListeners = [];
        this.state = CONNECTION_STATE.DISCONNECTED;
        this.attempt = 0;
        this.reconnectTimer = null;
        this.closed = true;
        this.token = null;
        // Commands waiting for a reply, keyed by the id they were sent with.
        this.pending = new Map();
        this.nextId = 1;
    }

    open(token = null) {
        if (!this.closed) return;
        this.closed = false;
        this.token = token;
        this.attempt = 0;
        this.connect();
    }

    connect() {
        if (this.closed) return;

        this.setState(CONNECTION_STATE.CONNECTING);
        // The token goes in the handshake rather than a first message, so an
        // unauthorised socket is refused before it can send anything.
        const url = this.token
            ? `${this.url}/?${TOKEN_PARAM}=${encodeURIComponent(this.token)}`
            : this.url;
        const ws = new WebSocket(url);
        ws.binaryType = 'arraybuffer';
        this.ws = ws;

        ws.onopen = () => {
            this.attempt = 0;
            this.setState(CONNECTION_STATE.CONNECTED);
        };

        ws.onmessage = event => {
            if (event.data instanceof ArrayBuffer) {
                const frame = decodeFrame(event.data);
                if (frame)
                    this.frameListeners.forEach(callback => callback(frame));
                return;
            }

            let msg = null;
            try {
                msg = JSON.parse(event.data);
            } catch {
                console.error('Invalid message from backend:', event.data);
                return;
            }

            // A reply belongs to one caller; everything else is an event for everyone.
            if (msg?.type === EVENT_TYPE.REPLY) {
                this.settle(msg);
                return;
            }
            this.listeners.forEach(callback => callback(msg));
        };

        ws.onerror = () => {};

        ws.onclose = () => {
            if (this.ws !== ws) return;
            this.ws = null;
            this.failPending('The engine connection was lost.');
            this.setState(CONNECTION_STATE.DISCONNECTED);
            this.scheduleReconnect();
        };
    }

    // Sends a command and resolves with { ok, value, error } once the engine answers.
    request(cmd, args) {
        if (this.ws?.readyState !== WebSocket.OPEN)
            return Promise.resolve({ ok: false, error: 'The engine is not connected.' });

        const id = String(this.nextId++);
        return new Promise(resolve => {
            const timer = setTimeout(() => {
                this.pending.delete(id);
                resolve({ ok: false, error: `The engine did not answer ${cmd}.` });
            }, REQUEST_TIMEOUT_MS);

            this.pending.set(id, { resolve, timer });
            this.ws.send(JSON.stringify(args === undefined ? { id, cmd } : { id, cmd, args }));
        });
    }

    settle(reply) {
        const entry = this.pending.get(reply.id);
        if (!entry) return;

        this.pending.delete(reply.id);
        clearTimeout(entry.timer);
        entry.resolve({ ok: reply.ok === true, value: reply.value, error: reply.error });
    }

    // A dropped socket answers every outstanding command rather than leaving the
    // caller's await hanging until its timeout.
    failPending(error) {
        this.pending.forEach(entry => {
            clearTimeout(entry.timer);
            entry.resolve({ ok: false, error });
        });
        this.pending.clear();
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

    addFrameListener(callback) {
        this.frameListeners.push(callback);
    }

    removeFrameListener(callback) {
        this.frameListeners = this.frameListeners.filter(cb => cb !== callback);
    }

    addStateListener(callback) {
        this.stateListeners.push(callback);
    }

    removeStateListener(callback) {
        this.stateListeners = this.stateListeners.filter(cb => cb !== callback);
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
        this.failPending('The engine connection was closed.');
        this.setState(CONNECTION_STATE.DISCONNECTED);
    }
}
