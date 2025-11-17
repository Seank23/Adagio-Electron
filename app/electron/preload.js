const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('api', {
    selectAudioFile: () => ipcRenderer.invoke('select-audio-file'),
    load: path => ipcRenderer.invoke('load-audio', path),
    play: () => ipcRenderer.invoke('play-audio'),
    pause: () => ipcRenderer.invoke('pause-audio'),
    wsUrl: () => 'ws://127.0.0.1:5001'
});