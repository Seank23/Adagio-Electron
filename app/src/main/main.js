const { app, BrowserWindow, dialog, ipcMain } = require('electron');
const { spawn } = require('child_process');
const path = require('path');

let engineProcess = null;

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

ipcMain.handle('select-audio-file', async () => {
    const { cancelled, filePaths } = await dialog.showOpenDialog({
        properties: ['openFile'],
        filters: [{ name: 'Audio Files', extensions: ['mp3', 'wav', 'flac'] }]
    });
    if (cancelled || filePaths.length === 0)
        return null;
    return filePaths[0];
});

app.whenReady().then(() => {
//     const enginePath = path.join(app.getAppPath(), 'engine', process.platform === 'win32'
//         ? 'AdagioEngine.exe'
//         : 'AdagioEngine'
//     );
//     engineProcess = spawn(enginePath, [], {
//         cwd: path.dirname(enginePath),
//         detached: false,
//         stdio: ['ignore', 'pipe', 'pipe']
//     });

//     engineProcess.stdout.on('data', chunk => {
//         console.log('[Engine]', chunk.toString());
//     });

//     engineProcess.stderr.on('data', (data) => {
//     console.error('[Engine error]', data.toString());
//   });

//     engineProcess.on('exit', code => {
//         console.log(`Engine process exited with code ${code}`);
//     });

    createWindow();
});