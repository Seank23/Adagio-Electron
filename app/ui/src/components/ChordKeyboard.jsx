import { useCallback, useEffect, useLayoutEffect, useRef } from 'react';
import Styled from '@emotion/styled';
import { createSelector } from '@reduxjs/toolkit';
import { getLatestSpectrum } from '../engine-client/FrameStore';
import { selectTuningHz } from '../store/pipelineSlice';
import { useStoreFrameListener } from '../hooks/useStoreFrameListener';
import { usePalette } from '../hooks/usePalette';
import { fitToElement } from '../utils/canvas';
import { makeAxis, spectrumMaxHz } from '../utils/frequencyAxis';
import { NOTE_NAMES, midiOf } from '../utils/music';
import { clamp } from '../utils/math';
import { measure } from '../utils/devPerf';
import { FONTS } from '../theme/tokens';

const WHITE_KEYS = [0, 2, 4, 5, 7, 9, 11];
const BLACK_KEYS = [[1, 1], [3, 2], [6, 4], [8, 5], [10, 6]];
const OCTAVES = 9;
const BLACK_HEIGHT = 26 / 44;
const BLACK_WIDTH = 0.6;

// A key is shaded once its score is a few percent of the loudest, and labelled once it's sounding.
const SHADE_THRESHOLD = 0.04;
const LABEL_THRESHOLD = 0.12;
const ON_ACCENT_THRESHOLD = 0.55;
const SCORE_EXPONENT = 0.8;
const LABEL_BASELINE = 5;
const DOT_BOTTOM = 19;

const selectKeyboardInputs = createSelector(
    [
        state => state.analysis.chordHistogram,
        state => state.analysis.notes,
        state => state.analysis.spectrumSR,
        selectTuningHz,
    ],
    (histogram, notes, spectrumRate, a4) => ({ histogram, notes, spectrumRate, a4 }),
);

const buildKeys = (axis, a4, height) => {
    const toX = midi => axis.toX(a4 * Math.pow(2, (midi - 69) / 12));
    const whites = [];
    const blacks = [];
    for (let octave = 0; octave < OCTAVES; octave++) {
        const cMidi = 12 * (octave + 1);
        const keyWidth = (toX(cMidi + 12) - toX(cMidi)) / 7;
        const left = toX(cMidi) - keyWidth / 2;
        WHITE_KEYS.forEach((pitchClass, i) => {
            const x = left + i * keyWidth;
            if (x + keyWidth >= 0 && x <= axis.width)
                whites.push({ x: x + 0.5, width: Math.max(1, keyWidth - 1), height, white: true, midi: cMidi + pitchClass });
        });
        BLACK_KEYS.forEach(([pitchClass, boundary]) => {
            const width = keyWidth * BLACK_WIDTH;
            const x = left + boundary * keyWidth - width / 2;
            if (x + width >= 0 && x <= axis.width)
                blacks.push({ x, width: Math.max(1, width), height: height * BLACK_HEIGHT, white: false, midi: cMidi + pitchClass });
        });
    }
    return [...whites, ...blacks].map(key => ({
        ...key,
        label: key.white ? `${NOTE_NAMES[key.midi % 12]}${Math.floor(key.midi / 12) - 1}` : NOTE_NAMES[key.midi % 12],
    }));
};

// Paints the keyboard for one analysis event. The key layout is kept in geometryRef and
// rebuilt only when the axis, the size or the tuning changes.
const drawKeyboard = (canvas, { histogram, notes, spectrumRate, a4 }, palette, geometryRef) => {
    const context = canvas.getContext('2d');
    const width = canvas.clientWidth;
    const height = canvas.clientHeight;
    const ratio = fitToElement(canvas);
    context.setTransform(ratio, 0, 0, ratio, 0, 0);
    context.clearRect(0, 0, width, height);

    // The spectrum's log axis whatever its scale, so every key has the same width. It
    // lines up with the spectrum's peaks only while the spectrum is on Log too.
    const axis = makeAxis({ maxHz: spectrumMaxHz(getLatestSpectrum(), spectrumRate), width, log: true });
    if (!axis)
        return;
    const id = `${axis.maxHz}|${width}|${height}|${a4}`;
    if (geometryRef.current.id !== id)
        geometryRef.current = { id, keys: buildKeys(axis, a4, height) };

    const weights = new Map();
    let maxWeight = 0;
    for (const { frequency, score } of histogram ?? []) {
        if (frequency <= 0)
            continue;
        const midi = midiOf(frequency, a4);
        const weight = (weights.get(midi) ?? 0) + score;
        weights.set(midi, weight);
        maxWeight = Math.max(maxWeight, weight);
    }
    const detected = new Set((notes ?? []).map(note => note.midi));

    context.fillStyle = palette.piano.gap;
    context.fillRect(0, 0, width, height);
    context.textAlign = 'center';
    context.textBaseline = 'alphabetic';

    for (const key of geometryRef.current.keys) {
        const t = maxWeight > 0 ? Math.pow((weights.get(key.midi) ?? 0) / maxWeight, SCORE_EXPONENT) : 0;
        const radius = key.white ? 3 : 2;
        const shape = new Path2D();
        shape.roundRect(key.x, 0, key.width, key.height, [0, 0, radius, radius]);
        context.fillStyle = key.white ? palette.piano.whiteKey : palette.piano.blackKey;
        context.fill(shape);
        if (t > SHADE_THRESHOLD) {
            context.globalAlpha = 0.15 + 0.85 * t;
            context.fillStyle = palette.accent.primaryFg;
            context.fill(shape);
            context.globalAlpha = 1;
        }

        const isDetected = detected.has(key.midi);
        const sounding = t > LABEL_THRESHOLD || isDetected;
        const centre = key.x + key.width / 2;
        if (sounding || (key.white && key.midi % 12 === 0)) {
            context.font = `${sounding ? 500 : 400} ${key.white ? 9 : 8}px ${FONTS.mono}`;
            // A narrow window can leave a key narrower than its label.
            if (context.measureText(key.label).width <= key.width) {
                context.fillStyle = sounding && t > ON_ACCENT_THRESHOLD
                    ? palette.text.onAccent
                    : key.white ? palette.piano.whiteKeyText : palette.piano.blackKeyText;
                context.globalAlpha = sounding && !isDetected ? 0.35 + 0.65 * t : 1;
                context.fillText(key.label, centre, key.height - LABEL_BASELINE);
                context.globalAlpha = 1;
            }
        }

        if (isDetected) {
            const diameter = clamp(key.width * 0.45, 4, 7);
            context.beginPath();
            context.arc(centre, key.height - DOT_BOTTOM - diameter / 2, diameter / 2, 0, Math.PI * 2);
            context.lineWidth = 1.5;
            context.strokeStyle = palette.bg.inset;
            context.stroke();
            context.fillStyle = palette.text.primary;
            context.fill();
        }
    }
};

const ChordKeyboard = () => {
    const palette = usePalette();
    const paletteRef = useRef(palette);
    const canvasRef = useRef(null);
    const geometryRef = useRef({ id: '', keys: [] });
    const inputsRef = useRef(undefined);

    const paint = useCallback(() => {
        const canvas = canvasRef.current;
        if (canvas && inputsRef.current)
            measure('keyboard draw', () => drawKeyboard(canvas, inputsRef.current, paletteRef.current, geometryRef));
    }, []);

    useStoreFrameListener(selectKeyboardInputs, inputs => {
        inputsRef.current = inputs;
        paint();
    });

    useLayoutEffect(() => {
        paletteRef.current = palette;
        paint();
    }, [palette, paint]);

    useEffect(() => {
        const canvas = canvasRef.current;
        if (!canvas)
            return;
        const observer = new ResizeObserver(paint);
        observer.observe(canvas);
        document.fonts?.ready.then(paint);
        return () => observer.disconnect();
    }, [paint]);

    return <Canvas ref={canvasRef} />;
};
export default ChordKeyboard;

const Canvas = Styled.canvas`
    position: absolute;
    inset: 0;
    width: 100%;
    height: 100%;
    display: block;
    border-radius: 4px;
`;
