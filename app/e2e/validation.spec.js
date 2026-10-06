// The overhaul guide's §7 validation, automated where the outcome is visible in the DOM
// or a canvas. What stays manual is listed in e2e/README.md.
const { test, expect } = require('@playwright/test');
const fs = require('fs');
const path = require('path');
const {
    HOP_MS, button, close, engineIndicator, inkOf, launch, makeFixtures, openFile, pitchClassRows, readout, removeUserData,
    resize, snapshot, startEngine, stepFrames, tap,
} = require('./harness');

let fixtures;
test.beforeAll(() => {
    fixtures = makeFixtures();
});
test.afterAll(() => {
    fs.rmSync(fixtures.dir, { recursive: true, force: true });
    removeUserData();
});

let session;
test.afterEach(async () => {
    await close(session?.app);
    session = null;
});

const position = page => readout(page, 'Position');

test('cold start without the engine: Connecting, then Disconnected, then connected with no reload', async () => {
    session = await launch({ engine: false });
    const { page } = session;
    await expect(engineIndicator(page)).toHaveText(/Connecting|Disconnected/);
    await expect(engineIndicator(page)).toHaveText('Disconnected');

    const engine = startEngine();
    try {
        await engine.ready;
        await expect(engineIndicator(page)).toHaveText('Engine connected', { timeout: 20000 });
        await expect(page.getByText('Ready. Open or drop a file to begin')).toBeVisible();
    } finally {
        await close(session.app);
        session = null;
        await engine.stop();
    }
});

test('open, play, pause, stop and skip back change only once the engine answers', async () => {
    session = await launch();
    const { app, page } = session;
    await expect(engineIndicator(page)).toHaveText('Engine connected');
    await openFile(app, page, fixtures.chord);
    await expect(page.getByText('WAV · 48 kHz · mono · 0:40')).toBeVisible();
    await expect(readout(page, 'Length')).toHaveText('00:40.000');

    await button(page, 'Play').click();
    await expect(button(page, 'Pause')).toBeVisible();
    await expect(position(page)).not.toHaveText('00:00.000');

    await button(page, 'Pause').click();
    await expect(button(page, 'Play')).toBeVisible();
    // A last position update can land just after the transport event; let it settle, then
    // it must stay put.
    let paused = '';
    await expect.poll(async () => {
        const before = await position(page).textContent();
        await page.waitForTimeout(150);
        paused = await position(page).textContent();
        return before === paused;
    }).toBe(true);
    await page.waitForTimeout(500);
    await expect(position(page)).toHaveText(paused);

    await button(page, 'Back to start').click();
    await expect(position(page)).toHaveText('00:00.000');

    await button(page, 'Play').click();
    await expect(position(page)).not.toHaveText('00:00.000');
    await button(page, 'Stop').click();
    await expect(position(page)).toHaveText('00:00.000');
    await expect(button(page, 'Play')).toBeVisible();
    // Stop leaves the transport at Ready, where Step is allowed.
    await expect(button(page, 'Step one analysis frame (.)')).toBeEnabled();
});

test('a reload mid-play brings back the track, the Pause icon, speed and volume', async () => {
    session = await launch();
    const { app, page } = session;
    await openFile(app, page, fixtures.chord);
    await button(page, '75').click();
    await expect(button(page, '75')).toHaveAttribute('aria-pressed', 'true');
    const volume = page.getByRole('slider', { name: 'Volume' });
    await volume.focus();
    await page.keyboard.press('ArrowRight');
    await page.keyboard.press('ArrowRight');
    await expect(volume).toHaveAttribute('aria-valuenow', '22');
    await button(page, 'Play').click();
    await expect(button(page, 'Pause')).toBeVisible();

    await page.reload();
    await expect(page.getByText(path.basename(fixtures.chord), { exact: true })).toBeVisible();
    await expect(page.getByText('WAV · 48 kHz · mono · 0:40')).toBeVisible();
    await expect(button(page, 'Pause')).toBeVisible();
    await expect(button(page, '75')).toHaveAttribute('aria-pressed', 'true');
    await expect(page.getByRole('slider', { name: 'Volume' })).toHaveAttribute('aria-valuenow', '22');
    // The waveform was sent with the load; the reload has to ask for it again.
    await expect.poll(() => inkOf(page, 'Track overview')).toBeGreaterThan(0.01);
});

test('the engine starts quiet: 20% volume on a fresh launch', async () => {
    session = await launch();
    const { page } = session;
    await expect(engineIndicator(page)).toHaveText('Engine connected');
    await expect(page.getByRole('slider', { name: 'Volume' })).toHaveAttribute('aria-valuenow', '20');
});

test('speed presets follow the engine, and the slider stops at 20%', async () => {
    session = await launch();
    const { app, page } = session;
    await openFile(app, page, fixtures.chord);
    await button(page, '50').click();
    await expect(page.getByText('50%', { exact: true })).toBeVisible();
    await expect(button(page, '50')).toHaveAttribute('aria-pressed', 'true');

    const speed = page.getByRole('slider', { name: 'Speed' });
    await speed.focus();
    await page.keyboard.press('Home');
    await expect(speed).toHaveAttribute('aria-valuenow', '20');
    await page.keyboard.press('ArrowLeft');
    await expect(speed).toHaveAttribute('aria-valuenow', '20');
    for (const preset of ['50', '75', '100'])
        await expect(button(page, preset)).toHaveAttribute('aria-pressed', 'false');
});

test('repeat restarts the track at its end; without it the transport returns to Ready', async () => {
    session = await launch();
    const { app, page } = session;
    await openFile(app, page, fixtures.short);

    // Pause first: right after the click the label is still Play, since nothing is optimistic.
    await button(page, 'Play').click();
    await expect(button(page, 'Pause')).toBeVisible();
    await expect(button(page, 'Play')).toBeVisible({ timeout: 5000 });
    await expect(position(page)).toHaveText('00:00.000');

    await tap(page, 'Repeat');
    await expect(button(page, 'Repeat')).toHaveAttribute('aria-pressed', 'true');
    await tap(page, 'Play');
    // Past the end of a 1.5 s track and still playing: it went round again.
    await page.waitForTimeout(2500);
    await expect(button(page, 'Pause')).toBeVisible();
    await tap(page, 'Repeat');
    await expect(button(page, 'Repeat')).toHaveAttribute('aria-pressed', 'false');
    await expect(button(page, 'Play')).toBeVisible({ timeout: 5000 });
    await expect(position(page)).toHaveText('00:00.000');
});

// The transport's readout of a time in ms: 2016 → '00:02.016'.
const clock = ms => `00:${String(Math.floor(ms / 1000)).padStart(2, '0')}.${String(Math.round(ms % 1000)).padStart(3, '0')}`;

test('step: each step is one hop, and 125 build the key up; reset empties the sidebar', async () => {
    session = await launch();
    const { app, page } = session;
    await openFile(app, page, fixtures.chord);
    await expect(page.getByText('—', { exact: true })).toBeVisible();

    await page.locator('body').click({ position: { x: 5, y: 300 } });
    for (let i = 0; i < 125; i++) {
        const expected = clock((i + 1) * HOP_MS);
        // The step gate sends at most one step every 33 ms and drops a press sooner than that.
        await page.waitForTimeout(35);
        await page.keyboard.press('.');
        await expect(position(page)).toHaveText(expected);
    }
    await expect(position(page)).toHaveText(clock(125 * HOP_MS));
    await expect(page.getByText('—', { exact: true })).toHaveCount(0);
    await expect(button(page, 'Play')).toBeVisible();

    await button(page, 'Reset analysis: clears the spectrum, key and chords').click();
    await expect(page.getByText('—', { exact: true })).toBeVisible();
    await expect.poll(() => inkOf(page, 'Spectrum')).toBeGreaterThan(0);

    await page.waitForTimeout(35);
    await page.keyboard.press('.');
    await expect(position(page)).toHaveText(clock(126 * HOP_MS));

    // While playing at 75%, a reset empties the sidebar and it fills again.
    await button(page, '75').click();
    await button(page, 'Play').click();
    await expect(page.getByText('—', { exact: true })).toHaveCount(0);
    await expect(button(page, 'Step one analysis frame (.)')).toBeDisabled();
    await button(page, 'Reset analysis: clears the spectrum, key and chords').click();
    await expect(page.getByText('—', { exact: true })).toHaveCount(0, { timeout: 5000 });
});

test('a seek keeps the analysis', async () => {
    session = await launch();
    const { app, page } = session;
    await openFile(app, page, fixtures.chord);
    await button(page, 'Play').click();
    await expect(page.getByText('—', { exact: true })).toHaveCount(0);
    const keyName = await page.getByRole('tabpanel').locator('section').first().locator('span').nth(1).textContent();
    await page.getByRole('group', { name: 'Waveform' }).click({ position: { x: 600, y: 10 } });
    await expect(page.getByText(keyName, { exact: true })).toBeVisible();
    await expect(page.getByText('—', { exact: true })).toHaveCount(0);
});

test('timeline: the overview zooms and pans, the waveform seeks on a click and ignores a drag', async () => {
    session = await launch();
    const { app, page } = session;
    await openFile(app, page, fixtures.chord);
    const range = page.getByText(/^\d+:\d\d – \d+:\d\d$/);
    await expect(range).toHaveText('0:00 – 0:40');

    const overview = page.getByRole('img', { name: 'Track overview' });
    const box = await overview.boundingBox();
    const cx = box.x + box.width / 2;
    const cy = box.y + box.height / 2;

    // Down zooms in, around the pressed time.
    await page.mouse.move(cx, cy);
    await page.mouse.down();
    await page.mouse.move(cx, cy + 300, { steps: 10 });
    await page.mouse.up();
    await expect(range).not.toHaveText('0:00 – 0:40');
    const zoomed = await range.textContent();

    // Sideways pans.
    await page.mouse.move(cx, cy);
    await page.mouse.down();
    await page.mouse.move(cx + 150, cy, { steps: 10 });
    await page.mouse.up();
    await expect(range).not.toHaveText(zoomed);

    // Up zooms back out to the whole track.
    await page.mouse.move(cx, cy);
    await page.mouse.down();
    await page.mouse.move(cx, cy - 2000, { steps: 20 });
    await page.mouse.up();
    await expect(range).toHaveText('0:00 – 0:40');

    const waveform = page.getByRole('group', { name: 'Waveform' });
    const wave = await waveform.boundingBox();
    const ruler = await page.getByRole('img', { name: 'Time ruler' }).boundingBox();
    expect(ruler.y + ruler.height).toBeLessThanOrEqual(wave.y + 0.5);

    await page.mouse.move(wave.x + wave.width * 0.25, wave.y + wave.height / 2);
    await page.mouse.down();
    await page.mouse.move(wave.x + wave.width * 0.6, wave.y + wave.height / 2, { steps: 10 });
    await page.mouse.up();
    await page.waitForTimeout(300);
    await expect(position(page)).toHaveText('00:00.000');

    await waveform.click({ position: { x: wave.width / 2, y: wave.height / 2 } });
    await expect(position(page)).toHaveText(/^00:(19|20|21)\.\d{3}$/);
});

test('the piano and the spectrum grid draw as soon as a track is open, before any analysis', async () => {
    session = await launch();
    const { app, page } = session;
    await openFile(app, page, fixtures.chord);
    await expect(page.getByText('—', { exact: true })).toBeVisible();
    await expect.poll(() => inkOf(page, 'Piano heatmap')).toBeGreaterThan(0.9);
    await expect.poll(() => inkOf(page, 'Spectrum')).toBeGreaterThan(0);
    await expect(page.getByText('50 Hz – 4 kHz')).toBeVisible();
});

test('switching to Linear redraws the spectrum and leaves the keyboard as it was', async () => {
    session = await launch();
    const { app, page } = session;
    await openFile(app, page, fixtures.chord);
    await stepFrames(page, 60);
    await expect(page.getByText('—', { exact: true })).toHaveCount(0);
    await page.waitForTimeout(200);

    const keyboard = await snapshot(page, 'Piano heatmap');
    const spectrum = await snapshot(page, 'Spectrum');
    await button(page, 'Linear').click();
    await expect(button(page, 'Linear')).toHaveAttribute('aria-pressed', 'true');
    await expect.poll(() => snapshot(page, 'Spectrum')).not.toBe(spectrum);
    expect(await snapshot(page, 'Piano heatmap')).toBe(keyboard);
});

test('sidebar: the chord badge matches the top chord, the tonic is marked, and the top bar fills 90%', async () => {
    session = await launch();
    const { app, page } = session;
    await openFile(app, page, fixtures.chord);
    await button(page, 'Play').click();
    await expect(page.getByText('—', { exact: true })).toHaveCount(0);
    await button(page, 'Pause').click();
    await page.waitForTimeout(200);

    const panel = page.getByRole('tabpanel');
    const keyName = await panel.locator('section').first().locator('span').nth(1).textContent();
    const rows = await pitchClassRows(page);
    const tonics = Object.entries(rows).filter(([, row]) => row.colour === 'var(--accent-secondary-text)');
    expect(tonics).toHaveLength(1);
    expect(keyName.split(' ')[0]).toBe(tonics[0][0]);
    const widths = Object.values(rows).map(row => parseFloat(row.width) || 0);
    expect(Math.max(...widths)).toBeCloseTo(90);

    const chordSection = panel.locator('section').nth(1);
    const topChord = await chordSection.locator(':scope > div').first().locator('span').first().textContent();
    await expect(page.getByText('Chord', { exact: true }).first().locator('xpath=following-sibling::*[1]')).toHaveText(topChord);

    // The arrows stay on the Analysis tab: Settings is disabled.
    await page.getByRole('tab', { name: 'Analysis' }).focus();
    await page.keyboard.press('ArrowRight');
    await expect(page.getByRole('tab', { name: 'Analysis' })).toBeFocused();
    await expect(page.getByRole('tab', { name: 'Settings' })).toBeDisabled();
});

test('tuning: a tone at 452.9 Hz is dropped at 440 and named A at 452.9; the value is clamped and kept', async () => {
    session = await launch();
    const { app, page } = session;
    await openFile(app, page, fixtures.sharp);
    const tuning = page.getByRole('slider', { name: 'A440 reference' });
    await expect(tuning).toHaveAttribute('aria-valuenow', '440');

    await stepFrames(page, 20);
    await page.waitForTimeout(200);
    await expect(page.getByText('—', { exact: true })).toBeVisible();

    // 440 → 452.9: 25 steps of 0.5 and 4 of 0.1.
    await tuning.focus();
    for (let i = 0; i < 25; i++)
        await page.keyboard.press('ArrowUp');
    for (let i = 0; i < 4; i++)
        await page.keyboard.press('Shift+ArrowUp');
    await expect(tuning).toHaveAttribute('aria-valuenow', '452.9');
    await expect(page.getByText('+50.0¢')).toBeVisible();

    await page.locator('body').click({ position: { x: 5, y: 300 } });
    await stepFrames(page, 20);
    await expect.poll(async () => (await pitchClassRows(page)).A.width).toBe('90%');

    // Clamped at the top of the range, and back to 440 on a double-click.
    await tuning.focus();
    for (let i = 0; i < 30; i++)
        await page.keyboard.press('ArrowUp');
    await expect(tuning).toHaveAttribute('aria-valuenow', '466');
    await expect(button(page, 'Raise the A4 reference')).toBeDisabled();
    await tuning.dblclick();
    await expect(tuning).toHaveAttribute('aria-valuenow', '440');

    await tuning.focus();
    await page.keyboard.press('ArrowDown');
    await expect(tuning).toHaveAttribute('aria-valuenow', '439.5');
    await page.reload();
    await expect(page.getByRole('slider', { name: 'A440 reference' })).toHaveAttribute('aria-valuenow', '439.5');
});

test('empty state: Close brings the drop card back, and an unsupported drop is refused', async () => {
    session = await launch();
    const { app, page } = session;
    await openFile(app, page, fixtures.chord);
    await button(page, 'Close file').click();
    await expect(page.getByText('Drop an audio file to analyse')).toBeVisible();
    await expect(page.getByText('No file open')).toBeVisible();

    await page.evaluate(() => {
        const transfer = new DataTransfer();
        transfer.items.add(new File(['x'], 'notes.txt', { type: 'text/plain' }));
        window.dispatchEvent(new DragEvent('drop', { dataTransfer: transfer, bubbles: true, cancelable: true }));
    });
    await expect(page.getByText('Adagio opens WAV, MP3 and FLAC files.')).toBeVisible();
    await expect(page).toHaveURL('http://localhost:5173/');
});

test('window size: nothing scrolls or overlaps at 1100×700, and a taller window grows every panel', async () => {
    session = await launch({ size: [1100, 700] });
    const { app, page } = session;
    await openFile(app, page, fixtures.chord);

    const overflow = await page.evaluate(() => [
        document.documentElement.scrollWidth - innerWidth,
        document.documentElement.scrollHeight - innerHeight,
    ]);
    expect(overflow).toEqual([0, 0]);

    // The play controls stay centred on the window, whatever the file name's length.
    const play = await button(page, 'Play').boundingBox();
    expect(Math.abs(play.x + play.width / 2 - 550)).toBeLessThan(40);
    const name = await page.getByText(path.basename(fixtures.chord), { exact: true }).boundingBox();
    const back = await button(page, 'Back to start').boundingBox();
    expect(name.x + name.width).toBeLessThanOrEqual(back.x);

    const timeline = page.getByText('Timeline', { exact: true }).locator('xpath=ancestor::*[3]');
    const heights = async () => ({
        timeline: (await timeline.boundingBox()).height,
        spectrum: (await page.getByRole('img', { name: 'Spectrum' }).boundingBox()).height,
        keyboard: (await page.getByRole('img', { name: 'Piano heatmap' }).boundingBox()).height,
    });
    const small = await heights();
    expect(small.timeline).toBeGreaterThanOrEqual(120);

    await resize(app, page, [1100, 1000]);
    await expect.poll(async () => {
        const tall = await heights();
        return tall.timeline > small.timeline && tall.spectrum > small.spectrum && tall.keyboard > small.keyboard;
    }).toBe(true);
});

const preferencesDialog = page => page.getByRole('dialog', { name: 'Preferences' });
const smoothing = page => preferencesDialog(page).getByRole('spinbutton', { name: 'Spectrum smoothing' });
const preferenceChip = (page, group, name) =>
    preferencesDialog(page).getByRole('group', { name: group }).getByRole('button', { name, exact: true });
const focusInDialog = page => page.evaluate(() => document.activeElement?.closest('[role="dialog"]') !== null);

test('preferences: Ctrl+, and the cog open them on any screen, Esc returns to the cog, the repo link opens the browser', async () => {
    session = await launch();
    const { app, page } = session;
    await expect(engineIndicator(page)).toHaveText('Engine connected');
    const dialog = preferencesDialog(page);
    const cog = button(page, 'Preferences (Ctrl+,)');

    // The empty screen.
    await page.locator('body').click({ position: { x: 5, y: 300 } });
    await page.keyboard.press('Control+,');
    await expect(dialog).toBeVisible();
    await expect.poll(() => focusInDialog(page)).toBe(true);
    await page.keyboard.press('Escape');
    await expect(dialog).toBeHidden();

    // While playing: the modal owns the keyboard, so Space doesn't pause behind it.
    await openFile(app, page, fixtures.chord);
    await button(page, 'Play').click();
    await expect(button(page, 'Pause')).toBeVisible();
    await cog.click();
    await expect(dialog).toBeVisible();
    await expect.poll(() => focusInDialog(page)).toBe(true);
    await page.keyboard.press('ArrowRight');
    await page.keyboard.press('Space');
    await page.waitForTimeout(300);
    await expect(button(page, 'Pause')).toBeVisible();
    await page.keyboard.press('Escape');
    await expect(dialog).toBeHidden();
    await expect(cog).toBeFocused();

    // Main hands github.com to the default browser and opens no window of its own.
    await app.evaluate(({ shell }) => {
        globalThis.openedExternally = [];
        shell.openExternal = async url => {
            globalThis.openedExternally.push(url);
        };
    });
    await cog.click();
    await expect(dialog.getByText(/^Version 0\.1\.0 · Updated \d{1,2} \w+ \d{4}$/)).toBeVisible();
    await dialog.getByRole('link', { name: /github\.com\/Seank23\/Adagio-Electron/ }).click();
    await expect.poll(() => app.evaluate(() => globalThis.openedExternally)).toEqual(['https://github.com/Seank23/Adagio-Electron']);
    expect(app.windows()).toHaveLength(1);
    await expect(page).toHaveURL('http://localhost:5173/');
});

test('preferences survive a restart: Light, 16 kHz, frame 8192, hop 256 and smoothing 6 reach the window and the engine', async () => {
    session = await launch();
    let { app, page } = session;
    const { userData } = session;
    await expect(engineIndicator(page)).toHaveText('Engine connected');
    await button(page, 'Preferences (Ctrl+,)').click();

    await preferenceChip(page, 'Theme', 'Light').click();
    await expect(page.locator('html')).toHaveAttribute('data-theme', 'light');
    await expect.poll(() => app.evaluate(({ nativeTheme }) => nativeTheme.themeSource)).toBe('light');
    for (const [group, name] of [['Sample rate', '16 kHz'], ['Frame size', '8192'], ['Hop size', '256']]) {
        await preferenceChip(page, group, name).click();
        await expect(preferenceChip(page, group, name)).toHaveAttribute('aria-pressed', 'true');
    }
    // Two steps up from the default of 4, each answered before the next.
    for (const count of ['5', '6']) {
        await preferencesDialog(page).getByRole('button', { name: 'Increase Spectrum smoothing' }).click();
        await expect(smoothing(page)).toHaveAttribute('aria-valuenow', count);
    }
    await expect(preferencesDialog(page).getByText('16 ms · 62.5 frames/s')).toBeVisible();
    await expect(preferencesDialog(page).getByText('last 96 ms')).toBeVisible();
    await expect(preferencesDialog(page).getByText('up to 8 kHz')).toBeVisible();
    await close(app);

    session = await launch({ userData });
    ({ app, page } = session);
    // Main set the theme before the window existed, so the page starts light.
    expect(await page.evaluate(() => matchMedia('(prefers-color-scheme: light)').matches)).toBe(true);
    await expect(page.locator('html')).toHaveAttribute('data-theme', 'light');
    await expect(engineIndicator(page)).toHaveText('Engine connected');

    await button(page, 'Preferences (Ctrl+,)').click();
    for (const [group, name] of [['Theme', 'Light'], ['Sample rate', '16 kHz'], ['Frame size', '8192'], ['Hop size', '256']])
        await expect(preferenceChip(page, group, name)).toHaveAttribute('aria-pressed', 'true');
    await expect(smoothing(page)).toHaveAttribute('aria-valuenow', '6');
    // antd moves focus in, and so starts listening for Esc, once its zoom has ended.
    await expect.poll(() => focusInDialog(page)).toBe(true);
    await page.keyboard.press('Escape');
    await expect(preferencesDialog(page)).toBeHidden();

    // 16000 / 256 is 62.5 frames a second.
    await openFile(app, page, fixtures.chord);
    await button(page, 'Play').click();
    await expect(page.getByText(/^Analysis (5[5-9]|6\d) frames\/s · 16 kHz · 8192 pt$/)).toBeVisible({ timeout: 5000 });
    await expect(page.getByText('50 Hz – 8 kHz')).toBeVisible();
    // Six frames 256 samples apart at 16 kHz.
    await expect(page.getByText('6-frame smoothing · 96 ms')).toBeVisible();
});
