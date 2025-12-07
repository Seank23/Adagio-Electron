const { app, BrowserWindow, dialog, ipcMain } = require('electron');
const { spawn } = require('child_process');
const path = require('path');

const isDev = !app.isPackaged;
let engineProcess = null;

function createWindow () {
    const mainWindow = new BrowserWindow({
        width: 1920,
        height: 1080,
        webPreferences: {
            preload: path.join(__dirname, 'preload.js')
        }
    });

    if (isDev) {
        mainWindow.loadURL('http://localhost:5173');
        mainWindow.webContents.openDevTools();
    } else {
        mainWindow.loadFile(path.join(__dirname, '../ui/index.html'));
    }
}

function spawnEngine() {
    const enginePath = path.join(app.getAppPath(), 'engine', process.platform === 'win32'
        ? 'AdagioEngine.exe'
        : 'AdagioEngine'
    );
    engineProcess = spawn(enginePath, [], {
        cwd: path.dirname(enginePath),
        detached: false,
        stdio: ['ignore', 'pipe', 'pipe']
    });

    engineProcess.stdout.on('data', chunk => {
        console.log('[Engine]', chunk.toString());
    });

    engineProcess.stderr.on('data', (data) => {
    console.error('[Engine error]', data.toString());
  });

    engineProcess.on('exit', code => {
        console.log(`Engine process exited with code ${code}`);
    });
}

app.whenReady().then(() => {
    if (!isDev)
        spawnEngine();
    createWindow();
    if (isDev) {
        ipcMain.on("ipc-message-sync", (event, channel, ...args) => {
            console.log("[ipc-message]", channel, args);
        });
        win.webContents.on("ipc-message", (event, channel, ...args) => {
            console.log("[renderer → main]", channel, args);
        });
    }
});

app.on("window-all-closed", () => {
  if (process.platform !== "darwin") app.quit();
});

ipcMain.handle('select-audio-file', async () => {
    const { cancelled, filePaths } = await dialog.showOpenDialog({
        properties: ['openFile'],
        filters: [{ name: 'Audio Files', extensions: ['mp3', 'wav', 'flac'] }]
    });
    if (cancelled || filePaths.length === 0)
        return null;
    return filePaths[0];
});

ipcMain.handle('load-audio', async (_, filePath) => {
    const res = await fetch('http://127.0.0.1:5000/load', { method: 'POST', body: filePath });
    return res.json();
});

ipcMain.handle('play-audio', async () => {
    const res = await fetch('http://127.0.0.1:5000/play', { method: 'POST' });
    return res.json();
});

ipcMain.handle('pause-audio', async () => {
    const res = await fetch('http://127.0.0.1:5000/pause', { method: 'POST' });
    return res.json();
});

ipcMain.handle('stop-audio', async () => {
    const res = await fetch('http://127.0.0.1:5000/stop', { method: 'POST' });
    return res.json();
});

ipcMain.handle('clear-audio', async () => {
    const res = await fetch('http://127.0.0.1:5000/clear', { method: 'POST' });
    return res.json();
});

ipcMain.handle('change-volume', async (_, volume) => {
    const res = await fetch('http://127.0.0.1:5000/volume', { method: 'POST', body: volume });
    return res.json();
});