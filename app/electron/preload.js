const { contextBridge, ipcRenderer, webUtils } = require('electron');

contextBridge.exposeInMainWorld('api', {
    selectAudioFile: () => ipcRenderer.invoke('select-audio-file'),
    // A dropped File's path on disk, or '' for one that has none.
    getPathForFile: file => {
        try {
            return webUtils.getPathForFile(file);
        } catch {
            return '';
        }
    },
    getEngineToken: () => ipcRenderer.invoke('engine-token'),
    onEngineStatus: callback => {
        const listener = (_, status) => callback(status);
        ipcRenderer.on('engine-status', listener);
        return () => ipcRenderer.removeListener('engine-status', listener);
    },
});
