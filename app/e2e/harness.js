// Fixtures and drivers for the end-to-end tests: WAV files written on the fly, the app
// launched with a real engine, and the few DOM readings the tests share.
const { _electron: electron, expect } = require('@playwright/test');
const { spawn } = require('child_process');
const fs = require('fs');
const os = require('os');
const path = require('path');

const APP_DIR = path.resolve(__dirname, '..');
const ENGINE_PATH = process.env.ADAGIO_ENGINE_PATH
    || path.resolve(APP_DIR, '../engine/build/Release/AdagioEngine.exe');

// 48 kHz, so one analysis hop (64 samples at 8 kHz) is exactly 384 source samples and
// 125 steps are exactly one second.
const SAMPLE_RATE = 48000;

const writeWav = (file, seconds, partials) => {
    const frames = Math.round(seconds * SAMPLE_RATE);
    const buffer = Buffer.alloc(44 + frames * 2);
    buffer.write('RIFF', 0, 'ascii');
    buffer.writeUInt32LE(36 + frames * 2, 4);
    buffer.write('WAVEfmt ', 8, 'ascii');
    buffer.writeUInt32LE(16, 16);
    buffer.writeUInt16LE(1, 20);
    buffer.writeUInt16LE(1, 22);
    buffer.writeUInt32LE(SAMPLE_RATE, 24);
    buffer.writeUInt32LE(SAMPLE_RATE * 2, 28);
    buffer.writeUInt16LE(2, 32);
    buffer.writeUInt16LE(16, 34);
    buffer.write('data', 36, 'ascii');
    buffer.writeUInt32LE(frames * 2, 40);
    const total = partials.reduce((sum, [, amplitude]) => sum + amplitude, 0);
    for (let i = 0; i < frames; i++) {
        const t = i / SAMPLE_RATE;
        // A short fade at each end, so the edges don't click into the spectrum.
        const fade = Math.min(1, t / 0.02, (seconds - t) / 0.02);
        const value = partials.reduce((sum, [hz, amplitude]) => sum + amplitude * Math.sin(2 * Math.PI * hz * t), 0);
        buffer.writeInt16LE(Math.round(0.6 * 32767 * fade * value / total), 44 + i * 2);
    }
    fs.writeFileSync(file, buffer);
    return file;
};

// A C major chord with a bass C: something for the key, the chord and the keyboard.
const C_MAJOR = [[130.81, 1], [261.63, 1], [329.63, 0.8], [392.0, 0.8]];

const makeFixtures = () => {
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'adagio-e2e-'));
    return {
        dir,
        chord: writeWav(path.join(dir, 'Chord in C with a long name for the top bar to cut short.wav'), 40, C_MAJOR),
        short: writeWav(path.join(dir, 'Short.wav'), 1.5, C_MAJOR),
        // A4 a quarter tone sharp: dropped at 440, named A at 452.9.
        sharp: writeWav(path.join(dir, 'A4 sharp.wav'), 20, [[452.9, 1]]),
        text: (() => {
            const file = path.join(dir, 'notes.txt');
            fs.writeFileSync(file, 'not audio');
            return file;
        })(),
    };
};

// Every launch gets its own userData, so one test's preferences.json can't reach the next.
// A test that relaunches passes the first launch's directory back in.
const USER_DATA_ROOT = path.join(os.tmpdir(), `adagio-e2e-user-data-${process.pid}`);
const makeUserData = () => {
    fs.mkdirSync(USER_DATA_ROOT, { recursive: true });
    return fs.mkdtempSync(path.join(USER_DATA_ROOT, 'launch-'));
};
const removeUserData = () => fs.rmSync(USER_DATA_ROOT, { recursive: true, force: true });

// Launches the app. With engine: true, main spawns the engine (with a token), as a
// packaged build does; otherwise main expects one started by hand.
const launch = async ({ engine = true, size, userData = makeUserData() } = {}) => {
    const env = { ...process.env, ADAGIO_E2E: '1', ADAGIO_USER_DATA: userData };
    if (engine)
        env.ADAGIO_ENGINE_PATH = ENGINE_PATH;
    else
        delete env.ADAGIO_ENGINE_PATH;

    const app = await electron.launch({
        executablePath: require('electron'),
        args: ['.'],
        cwd: APP_DIR,
        env,
    });
    const page = await app.firstWindow();
    await page.waitForLoadState('domcontentloaded');
    if (size)
        await resize(app, page, size);
    return { app, page, userData };
};

const close = async app => {
    // will-quit closes the engine's stdin and waits for it, which frees port 9001.
    await app?.close();
};

const resize = async (app, page, [width, height]) => {
    await app.evaluate(({ BrowserWindow }, [w, h]) => {
        BrowserWindow.getAllWindows()[0].setContentSize(w, h);
    }, [width, height]);
    await expect.poll(() => page.evaluate(() => [innerWidth, innerHeight])).toEqual([width, height]);
};

// An engine started by hand, with no token, as in dev.
const startEngine = () => {
    const child = spawn(ENGINE_PATH, [], { cwd: path.dirname(ENGINE_PATH), stdio: ['pipe', 'pipe', 'pipe'] });
    const ready = new Promise((resolve, reject) => {
        let output = '';
        child.stdout.on('data', chunk => {
            output += chunk;
            if (output.includes('Adagio engine ready'))
                resolve();
        });
        child.on('exit', code => reject(new Error(`engine exited with ${code}`)));
    });
    const stop = () => new Promise(resolve => {
        child.once('exit', resolve);
        child.stdin.end();
        setTimeout(() => child.kill(), 3000);
    });
    return { ready, stop };
};

// The dialog is main's; answering it there opens the file without a window.
const stubOpenDialog = (app, file) => app.evaluate(({ dialog }, filePath) => {
    dialog.showOpenDialog = async () => ({ canceled: false, filePaths: [filePath] });
}, file);

const engineIndicator = page => page.getByRole('status').filter({ hasText: /Engine|Connecting|Disconnected/ });

const button = (page, name) => page.getByRole('button', { name, exact: true });

// Clicks a button once no tooltip is open. A tooltip stays up while the pointer rests on
// its button, and the top bar's flip below the bar, over the transport row.
const tap = async (page, name) => {
    await page.mouse.move(5, 300);
    await expect(page.getByRole('tooltip').filter({ visible: true })).toHaveCount(0);
    await button(page, name).click();
};

const readout = (page, caption) => page.getByText(caption, { exact: true }).locator('xpath=following-sibling::*[1]');

const openFile = async (app, page, file) => {
    await stubOpenDialog(app, file);
    await page.getByRole('button', { name: /Open…/ }).click();
    await expect(page.getByText(path.basename(file), { exact: true })).toBeVisible();
    await expect(button(page, 'Play')).toBeEnabled();
};

// Presses '.' n times, no faster than the step gate's 33 ms, which drops a press sooner.
const stepFrames = async (page, n) => {
    for (let i = 0; i < n; i++) {
        await page.waitForTimeout(35);
        await page.keyboard.press('.');
    }
};

// The fraction of a canvas's pixels that are drawn (not transparent).
const inkOf = (page, name) => page.getByRole('img', { name }).evaluate(canvas => {
    const { width, height } = canvas;
    if (!width || !height)
        return 0;
    const data = canvas.getContext('2d').getImageData(0, 0, width, height).data;
    let drawn = 0;
    for (let i = 3; i < data.length; i += 4)
        if (data[i] > 0)
            drawn++;
    return drawn / (width * height);
});

const snapshot = (page, name) => page.getByRole('img', { name }).evaluate(canvas => canvas.toDataURL());

// The sidebar's pitch-class rows: note → { colour, width, percent }.
const pitchClassRows = page => page.getByRole('tabpanel').evaluate(panel => {
    const notes = ['C', 'Db', 'D', 'Eb', 'E', 'F', 'Gb', 'G', 'Ab', 'A', 'Bb', 'B'];
    const rows = {};
    panel.querySelectorAll('span').forEach(span => {
        const note = span.textContent;
        if (!notes.includes(note) || rows[note] || span.children.length)
            return;
        const row = span.parentElement;
        rows[note] = {
            colour: span.style.color,
            width: row.querySelector(':scope > div > div')?.style.width ?? '',
            percent: row.lastElementChild.textContent,
        };
    });
    return rows;
});

module.exports = {
    SAMPLE_RATE, button, close, engineIndicator, inkOf, launch, makeFixtures, openFile, pitchClassRows,
    readout, removeUserData, resize, snapshot, startEngine, stepFrames, stubOpenDialog, tap,
};
