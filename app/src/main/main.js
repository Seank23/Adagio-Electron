const { app, BrowserWindow } = require('electron');
const path = require('path');

function createWindow () {
    const mainWindow = new BrowserWindow({
        width: 1920,
        height: 1080,
        webPreferences: {
            preload: path.join(__dirname, '../preload/preload.js')
        }
    });

    mainWindow.loadFile(path.join(__dirname, '../../public/index.html'));
}

app.whenReady().then(() => {
    createWindow();
});