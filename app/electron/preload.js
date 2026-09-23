const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('api', {
    selectAudioFile: () => ipcRenderer.invoke('select-audio-file'),
    getEngineToken: () => ipcRenderer.invoke('engine-token'),
    onEngineStatus: callback => {
        const listener = (_, status) => callback(status);
        ipcRenderer.on('engine-status', listener);
        return () => ipcRenderer.removeListener('engine-status', listener);
    },
});
