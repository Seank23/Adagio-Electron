export class WebSocketEngine {
    constructor(url) {
        this.ws = new WebSocket(url);
        this.listeners = [];

        this.ws.onmessage = event => {
            let msg = null;
            try {
                msg = JSON.parse(event.data);
            } catch (e) {
                console.error("Invalid message from backend:", event.data);
                return;
            }
            this.listeners.forEach(callback => callback(msg));
        };

        this.ws.onopen = () => console.log('Connected to backend WebSocket');
        this.ws.onerror = err => console.error('WebSocket error:', err);
        this.ws.onclose = () => console.log('WebSocket closed');
    }

    addListener(callback) {
        this.listeners.push(callback);
    }

    removeListener(callback) {
        this.listeners = this.listeners.filter((cb) => cb !== callback);
    }

    send(data) {
        this.ws.send(JSON.stringify(data));
    }
}