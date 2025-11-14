const { contextBridge } = require('electron');

contextBridge.exposeInMainWorld('api', {
    analyzeAudio: async (data) => {
        const result = await fetch('http://127.0.0.1:5000/analyze', {
            method: 'POST',
            body: data
        });
        return result.json();
    }
});