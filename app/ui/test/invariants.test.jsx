// The guide's §5 invariants that are rules about the source itself, checked by reading
// it. Behaviour (render counts, what the router dispatches) is tested elsewhere.
import { describe, expect, it } from 'vitest';
import { readFileSync, readdirSync, statSync } from 'node:fs';
import { join, relative, sep } from 'node:path';

// Vitest runs from app/ui. (import.meta.url is not a file: URL under jsdom.)
const SRC = join(process.cwd(), 'src');

const walk = dir => readdirSync(dir).flatMap(name => {
    const path = join(dir, name);
    return statSync(path).isDirectory() ? walk(path) : [path];
});

const files = walk(SRC).map(path => ({
    path,
    rel: relative(SRC, path).split(sep).join('/'),
    text: readFileSync(path, 'utf8'),
}));
const sources = files.filter(file => /\.jsx?$/.test(file.rel));

// Comments may mention a colour or an API by name; only code counts.
const code = text => text
    .replace(/\/\*[\s\S]*?\*\//g, '')
    .replace(/(^|[^:'"`])\/\/.*$/gm, '$1');

const offenders = (predicate, list = sources) => list.filter(predicate).map(file => file.rel);

describe('source invariants', () => {
    it('uses .jsx for every source file', () => {
        expect(offenders(file => /\.(js|ts|tsx)$/.test(file.rel), files)).toEqual([]);
    });

    it('has no colour literal outside theme/tokens.jsx', () => {
        const colour = /#[0-9a-fA-F]{3,8}\b|\brgba?\s*\(|\bhsla?\s*\(/;
        expect(offenders(file => file.rel !== 'theme/tokens.jsx' && colour.test(code(file.text)))).toEqual([]);
    });

    it('never reads styles back with getComputedStyle', () => {
        expect(offenders(file => code(file.text).includes('getComputedStyle'))).toEqual([]);
    });

    it('never calls Math.clamp, which Electron 39 does not have', () => {
        expect(offenders(file => code(file.text).includes('Math.clamp'))).toEqual([]);
    });

    it('keeps every context in a file of its own, apart from its provider', () => {
        const contexts = sources.filter(file => code(file.text).includes('createContext('));
        expect(contexts.length).toBeGreaterThan(0);
        // A context module exports the context and nothing that renders.
        expect(offenders(file => /<[A-Z]/.test(code(file.text)), contexts)).toEqual([]);
    });

    it('lets only the router subscribe to engine events and frames', () => {
        const subscribes = /\buseEngine(Events|Frames)\s*\(/;
        expect(offenders(file => file.rel !== 'hooks/useEngineEvents.jsx' && subscribes.test(code(file.text))))
            .toEqual(['router/EngineEventRouter.jsx']);
    });

    it('lets only the router write to FrameStore', () => {
        const writers = /\b(publishSpectrum|resetSpectrum|stageWaveform|commitWaveform|clearFrames)\b/;
        expect(offenders(file => file.rel !== 'engine-client/FrameStore.jsx' && writers.test(code(file.text))))
            .toEqual(['router/EngineEventRouter.jsx']);
    });

    it('keeps components off the socket: only the engine client and its hooks use the context', () => {
        const allowed = new Set([
            'engine-client/WebSocketContext.jsx',
            'engine-client/WebSocketProvider.jsx',
            'hooks/useEngineCommands.jsx',
            'hooks/useEngineEvents.jsx',
        ]);
        expect(offenders(file => !allowed.has(file.rel) && code(file.text).includes('WebSocketContext'))).toEqual([]);
    });

    it('sizes every canvas it draws on with fitToElement', () => {
        const drawsOnCanvas = file => /getContext\(\s*'2d'\s*\)/.test(code(file.text));
        const canvases = sources.filter(drawsOnCanvas);
        expect(canvases.length).toBeGreaterThan(0);
        expect(offenders(file => !code(file.text).includes('fitToElement('), canvases)).toEqual([]);
    });

    it('gives tuningBand thresholds only: no colour in utils/music.jsx', () => {
        const music = files.find(file => file.rel === 'utils/music.jsx');
        expect(code(music.text)).not.toMatch(/#[0-9a-fA-F]{3,8}|palette|color/i);
    });

    it('selects no per-event data in the canvases', () => {
        // Analysis and playback reach them through useStoreListener / useStoreFrameListener;
        // useSelector is only for UI settings, which change on a click.
        const canvases = ['components/SpectrumCanvas.jsx', 'components/ChordKeyboard.jsx'];
        const perEvent = file => [...code(file.text).matchAll(/\buseSelector\s*\(([^)]*)\)/g)]
            .some(([, selector]) => !/^state => state\.settings\.\w+$/.test(selector.trim()));
        expect(offenders(file => canvases.includes(file.rel) && perEvent(file))).toEqual([]);
    });

    it('lets only PositionReadout and AudioTimeline follow currentTime through React', () => {
        const readers = sources.filter(file => /useSelector\([^)]*currentTime/.test(code(file.text)));
        expect(readers.map(file => file.rel)).toEqual(['components/TransportBar.jsx']);
        expect(code(readers[0].text)).toMatch(/const PositionReadout = \(\) => \{\s*const currentTime = useSelector/);
    });

    it('sets no volume from the UI on its own: the engine owns the initial volume', () => {
        // setVolume reaches the engine only from the volume slider.
        const callers = offenders(file => file.rel !== 'engine-client/EngineCommands.jsx' && /\bsetVolume\b/.test(code(file.text)));
        expect(callers).toEqual(['components/TransportBar.jsx']);
        const transport = files.find(file => file.rel === 'components/TransportBar.jsx');
        expect(code(transport.text).match(/setVolume/g)).toHaveLength(1);
        expect(code(transport.text)).toContain('useEngineSlider(engineVolume, commands.setVolume)');
    });
});
