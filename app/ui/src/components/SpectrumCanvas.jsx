import { useEffect, useRef } from 'react';
import Styled from '@emotion/styled';
import { useSelector } from 'react-redux';
import { getLatestSpectrum } from '../engine-client/FrameStore';
import { BINARY_FRAME } from '../utils/protocol';
import { measure } from '../utils/devPerf';
import { useStoreListener } from '../hooks/useStoreListener';
import { usePalette } from '../hooks/usePalette';
import { fitToElement, withAlpha } from '../utils/canvas';
import { formatHz, makeAxis, spectrumMaxHz } from '../utils/frequencyAxis';
import { formatCents, tuningBand } from '../utils/music';
import { FONTS } from '../theme/tokens';
import { selectAnalysisRate } from '../store/analysisSlice';

const selectNotes = state => state.analysis.notes;

// The band under the plot for the frequency labels, and the headroom above the tallest peak.
const AXIS_HEIGHT = 22;
const HEADROOM = 16;
// Notes are found to within a few bins of the peak the curve draws.
const PEAK_SEARCH_BINS = 2;
// Tags ease towards their peak rather than following every frame's height.
const LABEL_TIME_CONSTANT_MS = 20;
const TAG_RISE = 24;
const TAG_HEIGHT = 24;
const TAG_PADDING = 5;
const TAG_GAP = 4;

const NAME_FONT = `700 12px ${FONTS.mono}`;
const CENTS_FONT = `500 10px ${FONTS.mono}`;
const AXIS_FONT = `400 9px ${FONTS.mono}`;

// One rAF loop per mount reads the latest frame from FrameStore and redraws only when
// the frame, the notes or the canvas size have changed, or a tag is still easing.
const SpectrumCanvas = () => {
    const palette = usePalette();
    const showLogScale = useSelector(state => state.settings.showLogScale);
    const canvasRef = useRef(null);

    // Read by the draw loop, which notices the change itself; no render needed.
    const notesRef = useRef([]);
    useStoreListener(selectNotes, notes => {
        notesRef.current = notes ?? [];
    });
    const spectrumRateRef = useRef(0);
    useStoreListener(selectAnalysisRate, rate => {
        spectrumRateRef.current = rate || 0;
    });

    useEffect(() => {
        const canvas = canvasRef.current;
        const context = canvas?.getContext('2d');
        if (!context)
            return;

        let animationFrame;
        let drawnSpectrum;
        let drawnNotes;
        let drawnWidth = 0;
        let drawnHeight = 0;
        let drawnRate = 0;
        let labelsSettling = false;
        let lastDrawTime = performance.now();
        const labelY = new Map();

        const loop = now => {
            animationFrame = requestAnimationFrame(loop);
            measure('spectrum draw', () => render(now));
        };

        const render = now => {
            const spectrum = getLatestSpectrum();
            const notes = notesRef.current;
            const width = canvas.clientWidth;
            const height = canvas.clientHeight;
            const rate = spectrumRateRef.current;
            // A reset publishes a null frame, which counts as a change: it draws the empty grid.
            if (spectrum === drawnSpectrum && notes === drawnNotes && width === drawnWidth
                && height === drawnHeight && rate === drawnRate && !labelsSettling) {
                lastDrawTime = now;
                return;
            }
            drawnSpectrum = spectrum;
            drawnNotes = notes;
            drawnWidth = width;
            drawnHeight = height;
            drawnRate = rate;
            const elapsedMs = now - lastDrawTime;
            lastDrawTime = now;

            const ratio = fitToElement(canvas);
            context.setTransform(ratio, 0, 0, ratio, 0, 0);
            context.clearRect(0, 0, width, height);

            const axis = makeAxis({ maxHz: spectrumMaxHz(spectrum, rate), width, log: showLogScale });
            if (!axis) {
                labelY.clear();
                labelsSettling = false;
                return;
            }
            const plotHeight = height - AXIS_HEIGHT;

            drawGrid(axis, width, plotHeight);
            if (spectrum?.count > 0) {
                const maxMagnitude = spectrum.maxMagnitude || 1;
                // A Uint16 spectrum is magnitude / max at full scale; a Float32 one is the magnitude itself.
                const elementScale = spectrum.elementType === BINARY_FRAME.ELEMENT_TYPE.UINT16 ? maxMagnitude / 65535 : 1;
                const toY = magnitude => HEADROOM + (1 - Math.min(1, magnitude / maxMagnitude)) * (plotHeight - HEADROOM);
                drawSpectrum(spectrum, elementScale, axis, toY, plotHeight);
                drawNotes(notes, spectrum, elementScale, axis, toY, width, plotHeight, elapsedMs);
            } else {
                labelY.clear();
                labelsSettling = false;
            }
            drawAxisLabels(axis, width, plotHeight);
        };

        const drawGrid = (axis, width, plotHeight) => {
            context.fillStyle = palette.border.subtle;
            for (const hz of axis.ticks)
                context.fillRect(Math.round(axis.toX(hz)), 0, 1, plotHeight);
            for (const fraction of [0.25, 0.5, 0.75])
                context.fillRect(0, Math.round(plotHeight * fraction), width, 1);
            context.fillStyle = palette.border.strong;
            context.fillRect(0, Math.round(plotHeight), width, 1);
        };

        const drawSpectrum = (spectrum, elementScale, axis, toY, plotHeight) => {
            const data = spectrum.data;
            const binHz = spectrum.resolution;
            const path = new Path2D();
            let firstX = null;
            let lastX = 0;
            for (let i = 0; i < data.length; i++) {
                const hz = i * binHz;
                if (hz < axis.minHz || hz > axis.maxHz)
                    continue;
                const x = axis.toX(hz);
                const y = toY(data[i] * elementScale);
                if (firstX === null) {
                    path.moveTo(x, y);
                    firstX = x;
                } else {
                    path.lineTo(x, y);
                }
                lastX = x;
            }
            if (firstX === null)
                return;

            const area = new Path2D(path);
            area.lineTo(lastX, plotHeight);
            area.lineTo(firstX, plotHeight);
            area.closePath();
            const gradient = context.createLinearGradient(0, HEADROOM, 0, plotHeight);
            gradient.addColorStop(0, withAlpha(palette.accent.primaryFg, 0.28));
            gradient.addColorStop(1, withAlpha(palette.accent.primaryFg, 0.02));
            context.fillStyle = gradient;
            context.fill(area);

            context.lineWidth = 1.5;
            context.lineJoin = 'round';
            context.strokeStyle = palette.accent.primaryFg;
            context.stroke(path);
        };

        const peakMagnitude = (spectrum, elementScale, hz) => {
            const data = spectrum.data;
            const centre = Math.round(hz / spectrum.resolution);
            let peak = 0;
            for (let i = Math.max(0, centre - PEAK_SEARCH_BINS); i <= Math.min(data.length - 1, centre + PEAK_SEARCH_BINS); i++)
                peak = Math.max(peak, data[i]);
            return peak * elementScale;
        };

        const drawNotes = (notes, spectrum, elementScale, axis, toY, width, plotHeight, elapsedMs) => {
            labelsSettling = false;
            if (!notes?.length) {
                labelY.clear();
                return;
            }
            // Frame-rate independent: the same fraction of the gap closes per unit of time.
            const ease = 1 - Math.exp(-elapsedMs / LABEL_TIME_CONSTANT_MS);
            const seen = new Set();
            const placed = [];

            for (const note of notes) {
                const hz = note.frequency;
                if (!Number.isFinite(hz) || hz < axis.minHz || hz > axis.maxHz)
                    continue;
                const name = note.name || '';
                const cents = Number.isFinite(Number(note.errorCents)) ? Number(note.errorCents) : 0;
                const x = axis.toX(hz);
                const y = toY(peakMagnitude(spectrum, elementScale, hz));

                const targetY = y - TAG_RISE;
                const previousY = labelY.get(name);
                const easedY = previousY === undefined ? targetY : previousY + (targetY - previousY) * ease;
                labelY.set(name, easedY);
                seen.add(name);
                if (Math.abs(targetY - easedY) > 0.5)
                    labelsSettling = true;
                placed.push({ name, cents, x, y, tagY: easedY });
            }

            // Stems and markers first, so every tag sits above every marker.
            for (const { x, y } of placed) {
                context.globalAlpha = 0.35;
                context.fillStyle = palette.text.secondary;
                context.fillRect(Math.round(x), y, 1, plotHeight - y);
                context.globalAlpha = 1;

                context.beginPath();
                context.arc(x, y, 3.5, 0, Math.PI * 2);
                context.lineWidth = 2;
                context.strokeStyle = palette.bg.inset;
                context.stroke();
                context.fillStyle = palette.text.primary;
                context.fill();
            }

            context.textBaseline = 'middle';
            context.textAlign = 'left';
            for (const { name, cents, x, tagY } of placed) {
                const centsText = formatCents(cents);
                context.font = NAME_FONT;
                const nameWidth = context.measureText(name).width;
                context.font = CENTS_FONT;
                const centsWidth = context.measureText(centsText).width;
                const tagWidth = TAG_PADDING + nameWidth + TAG_GAP + centsWidth + TAG_PADDING;
                const left = Math.round(Math.min(x + 6, width - tagWidth - 4));
                const top = Math.round(Math.min(Math.max(4, tagY), plotHeight - TAG_HEIGHT - 2));

                context.beginPath();
                context.roundRect(left + 0.5, top + 0.5, tagWidth, TAG_HEIGHT, 4);
                context.fillStyle = withAlpha(palette.bg.panel, 0.9);
                context.fill();
                context.lineWidth = 1;
                context.strokeStyle = palette.border.strong;
                context.stroke();

                const middle = top + TAG_HEIGHT / 2 + 1;
                context.font = NAME_FONT;
                context.fillStyle = palette.text.primary;
                context.fillText(name, left + TAG_PADDING, middle);
                context.font = CENTS_FONT;
                context.fillStyle = palette.tuning[tuningBand(cents)];
                context.fillText(centsText, left + TAG_PADDING + nameWidth + TAG_GAP, middle);
            }

            // A note that returns later starts at its peak rather than easing in from where it left.
            for (const name of labelY.keys()) {
                if (!seen.has(name))
                    labelY.delete(name);
            }
        };

        const drawAxisLabels = (axis, width, plotHeight) => {
            context.font = AXIS_FONT;
            context.textBaseline = 'top';
            context.textAlign = 'left';
            context.fillStyle = palette.text.muted;
            const y = plotHeight + 6;
            const labels = [axis.minHz, ...axis.ticks].map(hz => ({ x: axis.toX(hz) + 4, text: formatHz(hz) }));
            const end = `${formatHz(axis.maxHz)} Hz`;
            const endX = width - context.measureText(end).width - 6;
            for (const { x, text } of labels) {
                if (x + context.measureText(text).width + 8 < endX)
                    context.fillText(text, x, y);
            }
            context.fillText(end, endX, y);
        };

        animationFrame = requestAnimationFrame(loop);
        // Canvas text measured before the fonts arrive is measured in the fallback.
        document.fonts?.ready.then(() => {
            drawnSpectrum = undefined;
        });
        return () => cancelAnimationFrame(animationFrame);
    }, [showLogScale, palette]);

    return <Canvas ref={canvasRef} />;
};
export default SpectrumCanvas;

const Canvas = Styled.canvas`
    position: absolute;
    inset: 0;
    width: 100%;
    height: 100%;
    display: block;
`;
