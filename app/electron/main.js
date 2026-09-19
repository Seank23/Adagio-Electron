const { app, BrowserWindow, dialog, ipcMain } = require('electron');
const { spawn } = require('child_process');
const path = require('path');

const ENGINE_HOST = 'http://127.0.0.1:5000';
const ENGINE_READY_LINE = 'Adagio engine ready';
const ENGINE_READY_TIMEOUT_MS = 10000;
const ENGINE_SHUTDOWN_TIMEOUT_MS = 3000;

const isDev = !app.isPackaged;

let mainWindow = null;
let engineProcess = null;
let engineStatus = { state: 'starting', error: '' };
let quitting = false;

const setEngineStatus = status => {
    engineStatus = { error: '', ...status };
    if (mainWindow && !mainWindow.isDestroyed())
        mainWindow.webContents.send('engine-status', engineStatus);
};

// Packaged builds get the binary from extraResources. In dev the engine is normally
// started by hand, unless ADAGIO_ENGINE_PATH points at a build of it.
const resolveEnginePath = () => {
    if (!app.isPackaged)
        return process.env.ADAGIO_ENGINE_PATH || null;

    const exe = process.platform === 'win32' ? 'AdagioEngine.exe' : 'AdagioEngine';
    return path.join(process.resourcesPath, 'engine', exe);
};

// Resolves once the engine prints its ready line, or with a failure the window can
// show. It never rejects: a missing engine must not stop the app from opening.
const spawnEngine = enginePath => new Promise(resolve => {
    let settled = false;
    let ready = false;
    let readyTimer = null;

    const settle = status => {
        if (settled)
            return;
        settled = true;
        ready = status.state === 'ready';
        clearTimeout(readyTimer);
        resolve(status);
    };

    let child = null;
    try {
        child = spawn(enginePath, [], {
            cwd: path.dirname(enginePath),
            detached: false,
            // stdin is a pipe on purpose: the engine shuts down when it reads EOF,
            // which is how stopEngine closes it without a forced kill.
            stdio: ['pipe', 'pipe', 'pipe']
        });
    } catch (error) {
        resolve({ state: 'failed', error: `${enginePath}: ${error.message}` });
        return;
    }

    engineProcess = child;

    // stdin is only ever closed, never written to, but a stream error must not become
    // an unhandled 'error' event during quit.
    child.stdin.on('error', () => {});

    let pending = '';
    child.stdout.on('data', chunk => {
        const lines = (pending + chunk.toString()).split(/\r?\n/);
        pending = lines.pop();
        for (const line of lines) {
            console.log('[Engine]', line);
            if (line.includes(ENGINE_READY_LINE))
                settle({ state: 'ready' });
        }
    });

    child.stderr.on('data', chunk => console.error('[Engine error]', chunk.toString().trimEnd()));

    // Without this listener a failed spawn is an unhandled 'error' event, which takes
    // the whole main process down with it.
    child.on('error', error => {
        if (engineProcess === child)
            engineProcess = null;
        settle({ state: 'failed', error: `${enginePath}: ${error.message}` });
    });

    child.on('exit', code => {
        console.log(`Engine process exited with code ${code}`);
        if (engineProcess === child)
            engineProcess = null;
        if (ready && !quitting)
            setEngineStatus({ state: 'exited', error: `Engine exited with code ${code}.` });
        settle({ state: 'failed', error: `Engine exited with code ${code} before it was ready.` });
    });

    readyTimer = setTimeout(
        () => settle({ state: 'failed', error: `Engine did not report ready within ${ENGINE_READY_TIMEOUT_MS / 1000} s.` }),
        ENGINE_READY_TIMEOUT_MS
    );
});

// An engine left running holds ports 5000 and 9001, and the next launch then fails to
// bind. Ask it to leave first, and kill it if it doesn't.
const stopEngine = () => new Promise(resolve => {
    const child = engineProcess;
    if (!child) {
        resolve();
        return;
    }
    engineProcess = null;

    const forceKill = setTimeout(() => child.kill(), ENGINE_SHUTDOWN_TIMEOUT_MS);
    child.once('exit', () => {
        clearTimeout(forceKill);
        resolve();
    });

    try {
        child.stdin.end();
    } catch {
        clearTimeout(forceKill);
        child.kill();
    }
});

const createWindow = () => {
    mainWindow = new BrowserWindow({
        width: 1920,
        height: 1080,
        webPreferences: {
            preload: path.join(__dirname, 'preload.js')
        }
    });

    mainWindow.on('closed', () => {
        mainWindow = null;
    });

    // The renderer mounts after the engine has been started, so it misses the status
    // that was set during startup. Replay it once the page is up.
    mainWindow.webContents.on('did-finish-load', () => {
        mainWindow.webContents.send('engine-status', engineStatus);
    });

    if (isDev) {
        mainWindow.webContents.on('ipc-message', (event, channel, ...args) => {
            console.log('[renderer -> main]', channel, args);
        });
        mainWindow.loadURL('http://localhost:5173');
        mainWindow.webContents.openDevTools();
    } else {
        // Vite builds to app/dist/ui; ui/index.html is the source page and points at
        // /src/main.jsx, which does not exist in a packaged app.
        mainWindow.loadFile(path.join(__dirname, '../dist/ui/index.html'));
    }
};

app.whenReady().then(async () => {
    const enginePath = resolveEnginePath();
    // No window yet, so this only records the status; createWindow replays it once the
    // renderer has loaded.
    setEngineStatus(enginePath
        ? await spawnEngine(enginePath)
        : { state: 'external' });
    createWindow();
});

app.on('window-all-closed', () => {
    if (process.platform !== 'darwin') app.quit();
});

app.on('will-quit', event => {
    if (!engineProcess)
        return;
    quitting = true;
    event.preventDefault();
    stopEngine().then(() => app.quit());
});

// Every relay answers { ok, value } or { ok, error }. A rejected fetch here would
// otherwise surface in the renderer as an unhandled rejection with no message.
const postToEngine = async (route, body) => {
    try {
        const res = await fetch(`${ENGINE_HOST}${route}`, body === undefined
            ? { method: 'POST' }
            : { method: 'POST', body: String(body) });
        const value = await res.json().catch(() => null);
        if (!res.ok)
            return { ok: false, error: value?.value ?? `Engine returned ${res.status}.` };
        return { ok: true, value };
    } catch (error) {
        return { ok: false, error: `Engine is not responding: ${error.message}` };
    }
};

ipcMain.handle('select-audio-file', async () => {
    const { canceled, filePaths } = await dialog.showOpenDialog({
        properties: ['openFile'],
        filters: [{ name: 'Audio Files', extensions: ['mp3', 'wav', 'flac'] }]
    });
    if (canceled || filePaths.length === 0)
        return null;
    return filePaths[0];
});

ipcMain.handle('load-audio', (_, filePath) => postToEngine('/load', filePath));
ipcMain.handle('play-audio', () => postToEngine('/play'));
ipcMain.handle('pause-audio', () => postToEngine('/pause'));
ipcMain.handle('stop-audio', () => postToEngine('/stop'));
ipcMain.handle('clear-audio', () => postToEngine('/clear'));
ipcMain.handle('change-volume', (_, volume) => postToEngine('/volume', volume));
ipcMain.handle('change-speed', (_, speed) => postToEngine('/speed', speed));
ipcMain.handle('seek-audio', (_, seconds) => postToEngine('/seek', seconds));
