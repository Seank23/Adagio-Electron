import { describe, expect, it, vi } from 'vitest';
import { formatHz, makeAxis, spectrumMaxHz } from '../src/utils/frequencyAxis';
import { clamp } from '../src/utils/math';
import { barWidths, formatPercent } from '../src/utils/bars';
import { centsFrom440, formatCents, midiOf, tuningBand } from '../src/utils/music';
import { fileNameOf, formatChannels, formatClock, formatDuration, formatOf, formatSampleRate } from '../src/utils/format';
import { coarsestPeaks, peakColumns } from '../src/utils/waveform';
import { decodeFrame } from '../src/engine-client/BinaryFrame';
import { BINARY_FRAME } from '../src/utils/protocol';
import {
    clearFrames, commitWaveform, getLatestSpectrum, getWaveform, publishSpectrum, resetSpectrum, stageWaveform,
    subscribeSpectrum, subscribeWaveform,
} from '../src/engine-client/FrameStore';

describe('frequency axis', () => {
    it('returns null without a top or a width, instead of NaN geometry', () => {
        expect(makeAxis({ maxHz: 0, width: 800, log: true })).toBeNull();
        expect(makeAxis({ maxHz: 4000, width: 0, log: true })).toBeNull();
        expect(makeAxis({ maxHz: 40, width: 800, log: false })).toBeNull();
    });

    it('maps 50 Hz to the left edge and the top to the right on both scales', () => {
        for (const log of [true, false]) {
            const axis = makeAxis({ maxHz: 4000, width: 800, log });
            expect(axis.toX(50)).toBeCloseTo(0);
            expect(axis.toX(4000)).toBeCloseTo(800);
            expect(axis.ticks.every(hz => hz > 50 && hz < 4000)).toBe(true);
        }
    });

    it('spaces octaves equally on the log scale, which the keyboard relies on', () => {
        const axis = makeAxis({ maxHz: 4000, width: 800, log: true });
        expect(axis.toX(220) - axis.toX(110)).toBeCloseTo(axis.toX(880) - axis.toX(440));
    });

    it('takes the top from a spectrum frame, or half the analysis rate without one', () => {
        expect(spectrumMaxHz(null, 8000)).toBe(4000);
        expect(spectrumMaxHz(null, 0)).toBe(0);
        const frame = { count: 2048, resolution: 8000 / 4096, data: new Uint16Array(2048) };
        expect(spectrumMaxHz(frame, 0)).toBeCloseTo(4000);
    });

    it('labels kHz compactly', () => {
        expect([formatHz(50), formatHz(1000), formatHz(1500)]).toEqual(['50', '1k', '1.5k']);
    });
});

describe('small helpers', () => {
    it('clamps, as Math.clamp would', () => {
        expect([clamp(-1, 0, 5), clamp(3, 0, 5), clamp(9, 0, 5)]).toEqual([0, 3, 5]);
    });

    it('fills 90% of the track for the largest bar, relative to its own list', () => {
        expect(barWidths([10, 5, 0])).toEqual(['90%', '45%', '0%']);
        expect(barWidths([0, 0])).toEqual(['0%', '0%']);
        expect(barWidths([0.4, 0.2])).toEqual(barWidths([40, 20]));
        expect(formatPercent(40.44)).toBe('40.4%');
        expect(formatPercent(undefined)).toBe('0.0%');
    });

    it('bands cents at 10 and 20, and formats them with a true minus', () => {
        expect([tuningBand(10), tuningBand(-10.1), tuningBand(20), tuningBand(20.5)]).toEqual(['in', 'near', 'near', 'off']);
        expect(formatCents(-4.2)).toBe('−4¢');
        expect(formatCents(14)).toBe('+14¢');
        expect(formatCents(centsFrom440(442), 1)).toBe('+7.9¢');
        expect(formatCents(0)).toBe('0¢');
    });

    it('names a pitch against the reference it is given', () => {
        expect(midiOf(440, 440)).toBe(69);
        // 452.9 Hz is a quarter tone sharp of A4 at 440, which is why a track tuned there loses its As.
        expect(midiOf(445, 440)).toBe(69);
        expect(midiOf(452.9, 452.9)).toBe(69);
        expect(midiOf(466.16, 440)).toBe(70);
        expect(midiOf(466.16, 466)).toBe(69);
    });

    it('formats the track meta line and the clock', () => {
        expect(fileNameOf('C:\\music\\Lady.mp3')).toBe('Lady.mp3');
        expect(fileNameOf('/home/me/Tone.WAV')).toBe('Tone.WAV');
        expect(formatOf('Tone.WAV')).toBe('WAV');
        expect(formatOf('noext')).toBe('');
        expect(formatSampleRate(44100)).toBe('44.1 kHz');
        expect(formatSampleRate(48000)).toBe('48 kHz');
        expect([formatChannels(1), formatChannels(2), formatChannels(6)]).toEqual(['mono', 'stereo', '6 ch']);
        expect(formatDuration(312)).toBe('5:12');
        expect(formatClock(84.38)).toBe('01:24.380');
        expect(formatClock(-1)).toBe('00:00.000');
    });

    it('scales overview columns to the loudest one', () => {
        const columns = peakColumns(Float32Array.from([0.1, -0.5, 0.25, 0.2]), 2);
        expect(Array.from(columns)).toEqual([1, 0.5]);
        expect(coarsestPeaks([{ resolution: 256 }, { resolution: 8192 }, { resolution: 512 }]).resolution).toBe(8192);
    });
});

const encode = ({ kind, elementType, generation = 0, values }) => {
    const { HEADER_SIZE, OFFSET, ELEMENT_TYPE } = BINARY_FRAME;
    const size = elementType === ELEMENT_TYPE.UINT16 ? 2 : 4;
    const buffer = new ArrayBuffer(HEADER_SIZE + values.length * size);
    const view = new DataView(buffer);
    view.setUint8(OFFSET.kind, kind);
    view.setUint8(OFFSET.version, 1);
    view.setUint16(OFFSET.elementType, elementType, true);
    view.setUint32(OFFSET.seekGeneration, generation, true);
    view.setFloat64(OFFSET.timestamp, 1.5, true);
    view.setFloat32(OFFSET.resolution, 1.953125, true);
    view.setFloat32(OFFSET.maxMagnitude, 2, true);
    view.setUint32(OFFSET.count, values.length, true);
    const Data = size === 2 ? Uint16Array : Float32Array;
    new Data(buffer, HEADER_SIZE, values.length).set(values);
    return buffer;
};

describe('binary frames', () => {
    it('decodes the header at the offsets protocol.json gives', () => {
        const { KIND, ELEMENT_TYPE } = BINARY_FRAME;
        const frame = decodeFrame(encode({ kind: KIND.SPECTRUM, elementType: ELEMENT_TYPE.UINT16, generation: 7, values: [1, 65535] }));
        expect(frame).toMatchObject({ kind: KIND.SPECTRUM, seekGeneration: 7, timestamp: 1.5, resolution: 1.953125, maxMagnitude: 2, count: 2 });
        expect(Array.from(frame.data)).toEqual([1, 65535]);
    });

    it('refuses a short buffer or one whose data is cut off', () => {
        const { KIND, ELEMENT_TYPE } = BINARY_FRAME;
        expect(decodeFrame(new ArrayBuffer(8))).toBeNull();
        const full = encode({ kind: KIND.WAVEFORM, elementType: ELEMENT_TYPE.FLOAT32, values: [0.5, 0.25] });
        expect(decodeFrame(full.slice(0, full.byteLength - 4))).toBeNull();
    });
});

describe('FrameStore', () => {
    it('drops a spectrum frame from an older seek generation than the one shown', () => {
        publishSpectrum({ seekGeneration: 3, id: 'new' });
        publishSpectrum({ seekGeneration: 2, id: 'stale' });
        expect(getLatestSpectrum().id).toBe('new');
        publishSpectrum({ seekGeneration: 3, id: 'next' });
        expect(getLatestSpectrum().id).toBe('next');
    });

    it('publishes null on a reset and tells the spectrum listeners, so the canvas redraws empty', () => {
        const listener = vi.fn();
        const unsubscribe = subscribeSpectrum(listener);
        publishSpectrum({ seekGeneration: 0 });
        resetSpectrum();
        expect(getLatestSpectrum()).toBeNull();
        expect(listener).toHaveBeenCalledTimes(2);
        unsubscribe();
    });

    it('publishes the waveform only once the whole set is in, and never an empty one', () => {
        const listener = vi.fn();
        const unsubscribe = subscribeWaveform(listener);
        stageWaveform({ resolution: 256, data: new Float32Array(4) });
        stageWaveform({ resolution: 512, data: new Float32Array(2) });
        expect(getWaveform()).toBeNull();
        commitWaveform();
        expect(getWaveform().map(set => set.resolution)).toEqual([256, 512]);
        commitWaveform();
        expect(getWaveform()).toHaveLength(2);
        expect(listener).toHaveBeenCalledTimes(1);
        clearFrames();
        expect(getWaveform()).toBeNull();
        unsubscribe();
    });
});
