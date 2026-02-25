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
});