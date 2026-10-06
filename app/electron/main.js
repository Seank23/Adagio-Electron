const { app, BrowserWindow, dialog, ipcMain, nativeTheme, shell } = require('electron');
const { spawn } = require('child_process');
const { randomBytes } = require('crypto');
const fs = require('fs');
const path = require('path');

const ENGINE_READY_LINE = 'Adagio engine ready';
const ENGINE_READY_TIMEOUT_MS = 10000;
const ENGINE_SHUTDOWN_TIMEOUT_MS = 3000;

// The window's colour before the page paints: bg/app from the UI's tokens.
const WINDOW_BACKGROUND = { dark: '#121212', light: '#E8E8E8' };

const isDev = !app.isPackaged;

// Before ready, so every path under userData moves with it: the e2e tests give each launch its own.
if (process.env.ADAGIO_USER_DATA)
    app.setPath('userData', process.env.ADAGIO_USER_DATA);

let mainWindow = null;
let engineProcess = null;
let engineStatus = { state: 'starting', error: '' };
let quitting = false;

// --- Preferences: app-wide, owned here because the window and the engine need them before any page exists.

const THEMES = ['system', 'dark', 'light'];
const ANALYSIS_KEYS = ['sampleRate', 'frameLength', 'hopSize'];
const preferencesFile = () => path.join(app.getPath('userData'), 'preferences.json');

// Field by field, so one bad value costs only itself. The analysis lists live in protocol.json, which
// isn't packaged, so only positive integers are kept here; the engine refuses anything else at startup.
const sanitisePreferences = raw => {
    const source = raw && typeof raw === 'object' ? raw : {};
    const analysisSource = source.analysis && typeof source.analysis === 'object' ? source.analysis : {};
    const analysis = {};
    for (const key of ANALYSIS_KEYS) {
        if (Number.isInteger(analysisSource[key]) && analysisSource[key] > 0)
            analysis[key] = analysisSource[key];
    }
    return {
        theme: THEMES.includes(source.theme) ? source.theme : 'system',
        analysis,
    };
};

const loadPreferences = () => {
    try {
        return sanitisePreferences(JSON.parse(fs.readFileSync(preferencesFile(), 'utf8')));
    } catch {
        return sanitisePreferences(null);
    }
};

// Written aside and renamed over the old file, so a crash mid-write can't leave half of one.
const savePreferences = prefs => {
    const file = preferencesFile();
    fs.mkdirSync(path.dirname(file), { recursive: true });
    fs.writeFileSync(`${file}.tmp`, JSON.stringify(prefs, null, 4));
    fs.renameSync(`${file}.tmp`, file);
};

let preferences = sanitisePreferences(null);

const windowBackground = () => WINDOW_BACKGROUND[nativeTheme.shouldUseDarkColors ? 'dark' : 'light'];

const ENGINE_ARGUMENTS = { sampleRate: '--sample-rate', frameLength: '--frame-length', hopSize: '--hop-size' };
const analysisArguments = analysis => Object.entries(ENGINE_ARGUMENTS)
    .filter(([key]) => analysis?.[key] !== undefined)
    .map(([key, flag]) => `${flag}=${analysis[key]}`);

// One secret per launch, handed to the engine on its command line and to the renderer through preload.
const engineToken = randomBytes(32).toString('hex');
let engineTokenInUse = null;

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
const spawnEngine = (enginePath, analysis) => new Promise(resolve => {
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
        child = spawn(enginePath, [`--token=${engineToken}`, ...analysisArguments(analysis)], {
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
    engineTokenInUse = engineToken;

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

// An engine left running holds port 9001, and the next launch then fails to bind.
// Ask it to leave first, and kill it if it doesn't.
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
        backgroundColor: windowBackground(),
        minWidth: 1100,
        minHeight: 700,
        webPreferences: {
            preload: path.join(__dirname, 'preload.js')
        }
    });

    mainWindow.on('closed', () => {
        mainWindow = null;
    });

    // No page opens a window of its own. A link to the repo goes to the default browser.
    mainWindow.webContents.setWindowOpenHandler(({ url }) => {
        if (url.startsWith('https://github.com/'))
            shell.openExternal(url);
        return { action: 'deny' };
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
        if (!process.env.ADAGIO_E2E)
            mainWindow.webContents.openDevTools();
    } else {
        // Vite builds to app/dist/ui; ui/index.html is the source page and points at
        // /src/main.jsx, which does not exist in a packaged app.
        mainWindow.loadFile(path.join(__dirname, '../dist/ui/index.html'));
    }
};

app.whenReady().then(async () => {
    preferences = loadPreferences();
    // Before the window, so its frame, prefers-color-scheme and its background agree from the first paint.
    nativeTheme.themeSource = preferences.theme;
    // A theme chosen in Preferences, or Windows switching while the theme is System.
    nativeTheme.on('updated', () => {
        if (mainWindow && !mainWindow.isDestroyed())
            mainWindow.setBackgroundColor(windowBackground());
    });

    const enginePath = resolveEnginePath();
    // No window yet, so this only records the status; createWindow replays it once the
    // renderer has loaded.
    setEngineStatus(enginePath
        ? await spawnEngine(enginePath, preferences.analysis)
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

ipcMain.handle('select-audio-file', async () => {
    const { canceled, filePaths } = await dialog.showOpenDialog({
        properties: ['openFile'],
        filters: [{ name: 'Audio Files', extensions: ['mp3', 'wav', 'flac'] }]
    });
    if (canceled || filePaths.length === 0)
        return null;
    return filePaths[0];
});

// Null when main did not start the engine: a hand-started engine in dev has no token.
ipcMain.handle('engine-token', () => engineTokenInUse);

ipcMain.handle('get-preferences', () => ({ ok: true, value: preferences }));

// A patch: { theme } or { analysis: { ... } }, merged over what is saved. The analysis values are
// the engine's answer, sent only after it accepted them; a hand-started engine never reads them.
ipcMain.handle('set-preferences', (_, patch) => {
    const next = sanitisePreferences({
        ...preferences,
        ...patch,
        analysis: { ...preferences.analysis, ...patch?.analysis },
    });
    try {
        savePreferences(next);
    } catch (error) {
        return { ok: false, error: `Couldn't save preferences: ${error.message}` };
    }
    if (next.theme !== preferences.theme)
        nativeTheme.themeSource = next.theme;
    preferences = next;
    return { ok: true, value: preferences };
});
