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
    // Both answer { ok, value } with the saved preferences, or { ok: false, error }.
    getPreferences: () => ipcRenderer.invoke('get-preferences'),
    setPreferences: patch => ipcRenderer.invoke('set-preferences', patch),
    onEngineStatus: callback => {
        const listener = (_, status) => callback(status);
        ipcRenderer.on('engine-status', listener);
        return () => ipcRenderer.removeListener('engine-status', listener);
    },
});
