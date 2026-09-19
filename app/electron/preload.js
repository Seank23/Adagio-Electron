const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('api', {
    selectAudioFile: () => ipcRenderer.invoke('select-audio-file'),
    load: path => ipcRenderer.invoke('load-audio', path),
    play: () => ipcRenderer.invoke('play-audio'),
    pause: () => ipcRenderer.invoke('pause-audio'),
    stop: () => ipcRenderer.invoke('stop-audio'),
    clear: () => ipcRenderer.invoke('clear-audio'),
    changeVolume: volume => ipcRenderer.invoke('change-volume', volume),
    changeSpeed: speed => ipcRenderer.invoke('change-speed', speed),
    seek: seconds => ipcRenderer.invoke('seek-audio', seconds),
    // Whether main could start the engine at all. The WebSocket says whether the UI
    // can reach it; this says why it cannot. Returns an unsubscribe function.
    onEngineStatus: callback => {
        const listener = (_, status) => callback(status);
        ipcRenderer.on('engine-status', listener);
        return () => ipcRenderer.removeListener('engine-status', listener);
    },
});
