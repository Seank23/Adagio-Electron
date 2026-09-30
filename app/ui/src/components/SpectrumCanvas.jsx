import React, { useEffect, useRef } from 'react';
import { selectIsFileOpen } from '../store/playbackSlice';
import { useSelector, useDispatch } from 'react-redux';
import { theme } from 'antd';
import { setCanvasWidth } from '../store/appSlice';
import { MIN_FREQ } from '../constants';
import { getLatestSpectrum } from '../engine-client/FrameStore';
import { BINARY_FRAME } from '../utils/protocol';
import { measure } from '../utils/devPerf';
import { useStoreListener } from '../hooks/useStoreListener';

const selectNotes = state => state.analysis.notes;

const X_AXIS_PADDING = 20;
// Notes are found to within a few bins of the peak the curve draws.
const PEAK_SEARCH_BINS = 2;
// Labels ease towards their peak rather than following every frame's height.
const LABEL_TIME_CONSTANT_MS = 20;
const LABEL_OFFSET = 12;
const MAX_Y_VALUES = [1, 10, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000, 50000, 100000, 200000, 500000, 1000000];

// The canvas runs one rAF loop per mount that reads the latest frame from FrameStore and redraws only when the frame or the notes have changed
const SpectrumCanvas = () => {
    const { token } = theme.useToken();
    const dispatch = useDispatch();

    const canvasRef = useRef(null);
    const canvasWidth = useSelector(state => state.app.canvasWidth);
    const isFileOpen = useSelector(selectIsFileOpen);
    const showLogScale = useSelector(state => state.settings.showLogScale);

    // Read by the draw loop, which notices the change itself; no render needed.
    const notesRef = useRef([]);
    useStoreListener(selectNotes, notes => {
        notesRef.current = notes;
    });

    useEffect(() => {
        const canvas = canvasRef.current;
        const parent = canvas?.parentElement;

        if (!parent) {
            return;
        }

        const updateWidth = () => {
            dispatch(setCanvasWidth(parent.clientWidth || 1000));
        };

        updateWidth();

        const observer = new ResizeObserver(updateWidth);
        observer.observe(parent);

        return () => observer.disconnect();
    }, [isFileOpen, dispatch]);

    useEffect(() => {
        const canvas = canvasRef.current;
        const context = canvas?.getContext('2d');
        if (!canvas || !context) {
            return;
        }

        let animationFrame;
        let drawnSpectrum;
        let drawnNotes;

        const labelY = new Map();
        let labelsSettling = false;
        let lastDrawTime = performance.now();

        const draw = (now = performance.now()) => {
            animationFrame = requestAnimationFrame(draw);
            measure('spectrum draw', () => render(now));
        };

        const render = now => {
            const spectrum = getLatestSpectrum();
            const notes = notesRef.current;
            if (spectrum === drawnSpectrum && notes === drawnNotes && !labelsSettling) {
                lastDrawTime = now;
                return;
            }
            drawnSpectrum = spectrum;
            drawnNotes = notes;
            const elapsedMs = now - lastDrawTime;
            lastDrawTime = now;

            context.clearRect(0, 0, canvas.width, canvas.height);
            if (!spectrum || spectrum.count === 0) {
                labelY.clear();
                labelsSettling = false;
                return;
            }

            const spectrumData = spectrum.data;
            const binHz = spectrum.resolution;
            const maxSpectrumHz = binHz * spectrumData.length;
            const meanMaxValue = spectrum.maxMagnitude || 1;
            // A Uint16 spectrum is magnitude / max at full scale; a Float32 one is the magnitude itself.
            const elementScale = spectrum.elementType === BINARY_FRAME.ELEMENT_TYPE.UINT16 ? spectrum.maxMagnitude / 65535 : 1;

            const freqToXLog = (freq, width) => {
                const minLog = Math.log10(MIN_FREQ);
                const maxLog = Math.log10(maxSpectrumHz);
                const logFreq = Math.log10(freq);

                return (
                    (logFreq - minLog) /
                    (maxLog - minLog)
                ) * width;
            };

            const freqToX = (freq, width) => {
                return (freq / maxSpectrumHz) * width;
            };

            const toX = freq => showLogScale ? freqToXLog(freq, canvas.width) : freqToX(freq, canvas.width);

            const magToY = (mag, height) => {
                return (height - X_AXIS_PADDING) - ((mag / meanMaxValue) * height) * 0.9;
            };

            drawSpectrum(spectrumData, binHz, maxSpectrumHz, elementScale, toX, magToY);
            drawNotes(notes, spectrumData, binHz, elementScale, maxSpectrumHz, toX, magToY, elapsedMs);
            drawXAxis(toX);
            drawYAxis(meanMaxValue, magToY);
        };

        const drawSpectrum = (spectrumData, binHz, maxSpectrumHz, elementScale, toX, magToY) => {
            context.lineWidth = 2;
            context.strokeStyle = token.colorPrimary;
            context.beginPath();

            for (let i = 0; i < spectrumData.length; i++) {
                const freq = i * binHz;
                if (freq < MIN_FREQ || freq > maxSpectrumHz) {
                    continue; // Skip frequencies outside the range
                }
                const x = toX(freq);
                const y = magToY(spectrumData[i] * elementScale, canvas.height);
                if (i === 0) {
                    context.moveTo(x, y);
                } else {
                    context.lineTo(x, y);
                }
            }
            context.lineTo(canvas.width, canvas.height - X_AXIS_PADDING);
            context.stroke();
        };

        const getNoteLabelColor = (errorCents) => {
            const absError = Math.abs(errorCents);

            if (absError <= 10) {
                return token.colorSuccess;
            }

            if (absError <= 20) {
                return token.colorWarning;
            }

            return token.colorError;
        };

        const peakMagnitude = (spectrumData, binHz, elementScale, freq) => {
            const centre = Math.round(freq / binHz);
            let peak = 0;
            for (let i = Math.max(0, centre - PEAK_SEARCH_BINS); i <= Math.min(spectrumData.length - 1, centre + PEAK_SEARCH_BINS); i++) {
                peak = Math.max(peak, spectrumData[i]);
            }
            return peak * elementScale;
        };

        const drawNotes = (notes, spectrumData, binHz, elementScale, maxSpectrumHz, toX, magToY, elapsedMs) => {
            const seen = new Set();
            labelsSettling = false;
            // Frame-rate independent: the same fraction of the gap closes per unit of time.
            const ease = 1 - Math.exp(-elapsedMs / LABEL_TIME_CONSTANT_MS);

            if (!notes || notes.length === 0) {
                labelY.clear();
                return;
            }

            context.font = '12px sans-serif';
            context.textAlign = 'left';
            context.textBaseline = 'middle';

            notes.forEach(note => {
                const freq = note.frequency;
                const mag = note.magnitude;
                const noteLabel = note.name || '';
                const parsedErrorCents = Number(note.errorCents);
                const errorCents = Number.isFinite(parsedErrorCents) ? parsedErrorCents : 0;
                const noteColor = getNoteLabelColor(errorCents);

                if (freq === undefined || mag === undefined || Number.isNaN(freq) || Number.isNaN(mag)) {
                    return;
                }

                if (freq < MIN_FREQ || freq > maxSpectrumHz) {
                    return;
                }

                const x = toX(freq);
                const y = magToY(peakMagnitude(spectrumData, binHz, elementScale, freq), canvas.height);

                context.fillStyle = noteColor;
                context.beginPath();
                context.arc(x, y, 3, 0, Math.PI * 2);
                context.fill();

                const targetY = y - LABEL_OFFSET;
                const previousY = labelY.get(noteLabel);
                const easedY = previousY === undefined ? targetY : previousY + (targetY - previousY) * ease;
                labelY.set(noteLabel, easedY);
                seen.add(noteLabel);
                if (Math.abs(targetY - easedY) > 0.5) {
                    labelsSettling = true;
                }

                const textX = Math.min(x + 6, canvas.width - 36);
                const textY = Math.max(8, Math.min(easedY, canvas.height - X_AXIS_PADDING - 8));

                context.strokeStyle = token.colorBgContainer;
                context.lineWidth = 3;
                context.strokeText(noteLabel, textX, textY);
                context.fillStyle = noteColor;
                context.fillText(noteLabel, textX, textY);
            });

            // A note that returns later starts at its peak rather than easing in from where it left.
            for (const name of labelY.keys()) {
                if (!seen.has(name)) {
                    labelY.delete(name);
                }
            }
        };

        const drawXAxis = (toX) => {
            context.fillStyle = '#fff';
            context.fillRect(0, canvas.height - X_AXIS_PADDING, canvas.width, X_AXIS_PADDING);

            context.strokeStyle = "#666";
            context.fillStyle = "#888";
            context.font = "12px sans-serif";

            context.beginPath();
            context.moveTo(0, canvas.height - X_AXIS_PADDING);
            context.lineTo(canvas.width, canvas.height - X_AXIS_PADDING);
            context.stroke();

            const logTicks = [50, 100, 200, 300, 500, 1000, 2000, 3000, 5000];
            const linearTicks = [0, 500, 1000, 1500, 2000, 2500, 3000, 3500, 4000, 4500, 5000];
            const ticks = showLogScale ? logTicks : linearTicks;
            ticks.forEach(freq => {
                const x = toX(freq);

                context.beginPath();
                context.moveTo(x, canvas.height - X_AXIS_PADDING);
                context.lineTo(x, canvas.height - X_AXIS_PADDING + 5);
                context.stroke();

                context.fillText(
                    freq >= 1000 ? `${freq / 1000}k` : `${freq}`,
                    x + 2,
                    canvas.height - 4
                );
            });
        };

        const drawYAxis = (meanMaxValue, magToY) => {
            context.strokeStyle = "#666";
            context.fillStyle = "#aaa";
            context.font = "12px sans-serif";

            context.beginPath();
            context.moveTo(0, 0);
            context.lineTo(0, canvas.height);
            context.stroke();
            const ticksCount = 5;
            const roundedMax = MAX_Y_VALUES.find(val => val >= meanMaxValue) || meanMaxValue;

            for (let mag = 0; mag <= roundedMax; mag += roundedMax / ticksCount) {
                if (mag === 0) continue;
                const y = magToY(mag, canvas.height);

                context.beginPath();
                context.moveTo(0, y);
                context.lineTo(5, y);
                context.stroke();

                context.fillText(mag, 8, y + 4);
            }
        };

        draw();
        return () => cancelAnimationFrame(animationFrame);
    }, [isFileOpen, showLogScale, canvasWidth, token.colorPrimary, token.colorError, token.colorBgContainer, token.colorSuccess, token.colorWarning]);

    return (
        <>
            {isFileOpen && (
                <>
                    <canvas ref={canvasRef} width={canvasWidth} height={300} />
                </>
            )}
        </>
    );
}
export default SpectrumCanvas;
