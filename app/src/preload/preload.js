const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('api', {
    selectAudioFile: () => ipcRenderer.invoke('select-audio-file'),
    load: async path => {
        const res = await fetch('http://127.0.0.1:5000/load', { method: 'POST', body: path });
        return res.json();
    },
    play: async () => {
        const res = await fetch('http://127.0.0.1:5000/play', { method: 'POST' });
        return res.json();
    },
    pause: async () => {
        const res = await fetch('http://127.0.0.1:5000/pause', { method: 'POST' });
        return res.json();
    },
    wsUrl: () => 'ws://127.0.0.1:5001'
});