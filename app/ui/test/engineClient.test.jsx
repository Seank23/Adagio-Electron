import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import { CONNECTION_STATE, WebSocketEngine } from '../src/engine-client/WebSocketEngine';

// A WebSocket the test drives by hand.
class FakeSocket {
    static OPEN = 1;
    static instances = [];

    constructor(url) {
        this.url = url;
        this.readyState = 0;
        this.sent = [];
        FakeSocket.instances.push(this);
    }

    send(data) { this.sent.push(JSON.parse(data)); }
    close() { this.readyState = 3; this.onclose?.(); }
    open() { this.readyState = FakeSocket.OPEN; this.onopen?.(); }
    receive(data) { this.onmessage({ data: typeof data === 'string' ? data : JSON.stringify(data) }); }
    drop() { this.readyState = 3; this.onclose?.(); }
}

describe('WebSocketEngine: every command answers { ok, value, error } and never throws', () => {
    let RealWebSocket;
    beforeEach(() => {
        RealWebSocket = globalThis.WebSocket;
        globalThis.WebSocket = FakeSocket;
        FakeSocket.instances = [];
    });
    afterEach(() => {
        globalThis.WebSocket = RealWebSocket;
        vi.useRealTimers();
    });

    const connected = token => {
        const engine = new WebSocketEngine('ws://engine');
        engine.open(token);
        const socket = FakeSocket.instances.at(-1);
        socket.open();
        return { engine, socket };
    };

    it('answers a command sent while disconnected at once, with ok false', async () => {
        const engine = new WebSocketEngine('ws://engine');
        await expect(engine.request('play')).resolves.toEqual({ ok: false, error: 'The engine is not connected.' });
    });

    it('puts the token in the handshake, not in a message', () => {
        const { socket } = connected('a b');
        expect(socket.url).toBe('ws://engine/?token=a%20b');
        expect(socket.sent).toEqual([]);
    });

    it('routes a reply to the caller that sent it, and events to every listener', async () => {
        const { engine, socket } = connected();
        const events = vi.fn();
        engine.addListener(events);
        const play = engine.request('play');
        const seek = engine.request('seek', 2);
        expect(socket.sent).toEqual([{ id: '1', cmd: 'play' }, { id: '2', cmd: 'seek', args: 2 }]);

        socket.receive({ type: 'reply', id: '2', ok: false, error: 'No file.' });
        socket.receive({ type: 'transport', value: { state: 'playing' } });
        socket.receive({ type: 'reply', id: '1', ok: true, value: null });

        await expect(seek).resolves.toEqual({ ok: false, value: undefined, error: 'No file.' });
        await expect(play).resolves.toEqual({ ok: true, value: null, error: undefined });
        expect(events).toHaveBeenCalledTimes(1);
        expect(events).toHaveBeenCalledWith({ type: 'transport', value: { state: 'playing' } });
    });

    it('answers every command in flight when the socket drops, then reconnects with backoff', async () => {
        vi.useFakeTimers();
        const { engine, socket } = connected();
        const states = [];
        engine.addStateListener(state => states.push(state));
        const pending = [engine.request('play'), engine.request('status')];
        socket.drop();
        await expect(Promise.all(pending)).resolves.toEqual([
            { ok: false, error: 'The engine connection was lost.' },
            { ok: false, error: 'The engine connection was lost.' },
        ]);
        expect(states).toEqual([CONNECTION_STATE.DISCONNECTED]);

        vi.advanceTimersByTime(1000);
        expect(FakeSocket.instances).toHaveLength(2);
        expect(states).toEqual([CONNECTION_STATE.DISCONNECTED, CONNECTION_STATE.CONNECTING]);
        engine.close();
    });

    it('ignores a message that is not JSON instead of throwing', () => {
        const { engine, socket } = connected();
        const events = vi.fn();
        engine.addListener(events);
        vi.spyOn(console, 'error').mockImplementation(() => {});
        expect(() => socket.receive('not json')).not.toThrow();
        expect(events).not.toHaveBeenCalled();
    });
});
