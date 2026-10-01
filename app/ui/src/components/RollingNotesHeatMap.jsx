import React, { useEffect, useRef } from 'react';
import { clamp01, toRgbChannels } from '../utils/utils';
import { useStoreListener } from '../hooks/useStoreListener';
import { measure } from '../utils/devPerf';
import { fitToElement } from '../utils/canvas';
import { usePalette } from '../hooks/usePalette';

const DEFAULT_MIN_FREQ = 50;
const DEFAULT_MAX_FREQ = 22050;
const DENSITY_SMOOTHING = 0.001;
const MAX_RENDER_SAMPLES = 1024;
const HOTSPOT_GAIN = 4.0;
const HOTSPOT_EXPONENT = 0.8;
const STRIP_HEIGHT = 42;
const TICK_HEIGHT = 16;
const NOTE_NAMES = ['C', 'Db', 'D', 'Eb', 'E', 'F', 'Gb', 'G', 'Ab', 'A', 'Bb', 'B'];

const intensityOf = (score, maxScore) =>
    maxScore > 0 ? clamp01(Math.pow((score / maxScore) * HOTSPOT_GAIN, HOTSPOT_EXPONENT)) : 0;

const RollingNotesHeatMap = ({ histogramSelector, width, minFreq = DEFAULT_MIN_FREQ, maxFreq = DEFAULT_MAX_FREQ, showLogScale = false }) => {
    const palette = usePalette();
    const stripRef = useRef(null);
    const ticksRef = useRef(null);
    const histogramRef = useRef([]);
    const drawRef = useRef(() => {});
    const frameRef = useRef(0);

    useStoreListener(histogramSelector, histogram => {
        histogramRef.current = histogram || [];
        if (frameRef.current) return;
        frameRef.current = requestAnimationFrame(() => {
            frameRef.current = 0;
            measure('heat map draw', () => drawRef.current(histogramRef.current));
        });
    });

    useEffect(() => () => {
        cancelAnimationFrame(frameRef.current);
        frameRef.current = 0;
    }, []);

    // Everything that depends only on the geometry and the theme is worked out here,
    // once, and the per-event draw is left with the histogram itself.
    useEffect(() => {
        const strip = stripRef.current;
        const ticks = ticksRef.current;
        if (!strip || !ticks) return;

        const safeMin = Number.isFinite(minFreq) ? minFreq : DEFAULT_MIN_FREQ;
        const safeMax = Number.isFinite(maxFreq) && maxFreq > safeMin ? maxFreq : DEFAULT_MAX_FREQ;
        const span = Math.max(1e-6, safeMax - safeMin);
        const logMin = Math.log10(safeMin);
        const logSpan = Math.max(1e-6, Math.log10(safeMax) - logMin);
        const toFraction = frequency => clamp01(showLogScale
            ? (Math.log10(frequency) - logMin) / logSpan
            : (frequency - safeMin) / span);

        const targetWidth = Number.isFinite(width) ? width : 1000;
        const sampleCount = Math.max(120, Math.min(MAX_RENDER_SAMPLES, Math.round(targetWidth * 0.85)));
        const sampleMaxIndex = Math.max(1, sampleCount - 1);
        const sigmaInSamples = Math.max(1, DENSITY_SMOOTHING * sampleMaxIndex);
        const activeRadius = Math.max(1, Math.ceil(sigmaInSamples * 3));
        const twoSigmaSquared = 2 * sigmaInSamples * sigmaInSamples;
        const samples = new Float32Array(sampleCount);

        // The ramp runs from the plot's inset background to the accent.
        const [lowRed, lowGreen, lowBlue] = toRgbChannels(palette.bg.inset);
        const [highRed, highGreen, highBlue] = toRgbChannels(palette.accent.primaryFg);
        const heatChannel = (low, high, t) => Math.round(low + ((high - low) * t));
        // Labels sit on the panel, so theirs starts from the panel: a note that isn't sounding has no label.
        const [labelRed, labelGreen, labelBlue] = toRgbChannels(palette.bg.panel);
        const labelColor = t => `rgb(${heatChannel(labelRed, highRed, t)}, ${heatChannel(labelGreen, highGreen, t)}, ${heatChannel(labelBlue, highBlue, t)})`;

        // One pixel per sample, stretched across the strip with smoothing on
        const row = document.createElement('canvas');
        row.width = sampleCount;
        row.height = 1;
        const rowContext = row.getContext('2d');
        const rowImage = rowContext.createImageData(sampleCount, 1);

        const noteTicks = [];
        const lowerMidi = Math.max(0, Math.floor(69 + (12 * Math.log2(safeMin / 440))));
        const upperMidi = Math.min(127, Math.ceil(69 + (12 * Math.log2(safeMax / 440))));
        for (let midi = lowerMidi; midi <= upperMidi; midi += 1) {
            const frequency = 440 * Math.pow(2, (midi - 69) / 12);
            if (frequency < safeMin || frequency > safeMax) continue;
            const fraction = toFraction(frequency);
            noteTicks.push({
                label: `${NOTE_NAMES[midi % 12]}${Math.floor(midi / 12) - 1}`,
                fraction,
                sampleIndex: Math.round(fraction * sampleMaxIndex),
            });
        }

        drawRef.current = histogram => {
            samples.fill(0);
            histogram.forEach(entry => {
                if (entry.frequency < safeMin || entry.frequency > safeMax) return;
                const centerIndex = Math.round(toFraction(entry.frequency) * sampleMaxIndex);
                const startIndex = Math.max(0, centerIndex - activeRadius);
                const endIndex = Math.min(sampleMaxIndex, centerIndex + activeRadius);
                for (let index = startIndex; index <= endIndex; index += 1) {
                    const delta = index - centerIndex;
                    samples[index] += entry.score * Math.exp(-(delta * delta) / twoSigmaSquared);
                }
            });

            let maxScore = 0;
            for (let index = 0; index < sampleCount; index += 1) {
                maxScore = Math.max(maxScore, samples[index]);
            }

            const pixels = rowImage.data;
            for (let index = 0; index < sampleCount; index += 1) {
                const t = intensityOf(samples[index], maxScore);
                pixels[index * 4] = heatChannel(lowRed, highRed, t);
                pixels[index * 4 + 1] = heatChannel(lowGreen, highGreen, t);
                pixels[index * 4 + 2] = heatChannel(lowBlue, highBlue, t);
                pixels[index * 4 + 3] = 255;
            }
            rowContext.putImageData(rowImage, 0, 0);

            fitToElement(strip);
            const stripContext = strip.getContext('2d');
            stripContext.imageSmoothingEnabled = true;
            stripContext.drawImage(row, 0, 0, strip.width, strip.height);

            const ratio = fitToElement(ticks);
            const tickContext = ticks.getContext('2d');
            tickContext.setTransform(ratio, 0, 0, ratio, 0, 0);
            tickContext.clearRect(0, 0, ticks.clientWidth, TICK_HEIGHT);
            tickContext.font = '11px sans-serif';
            tickContext.textAlign = 'center';
            tickContext.textBaseline = 'middle';
            noteTicks.forEach(tick => {
                tickContext.fillStyle = labelColor(intensityOf(samples[tick.sampleIndex], maxScore));
                tickContext.fillText(tick.label, tick.fraction * ticks.clientWidth, TICK_HEIGHT / 2);
            });
        };

        measure('heat map draw', () => drawRef.current(histogramRef.current));
    }, [width, minFreq, maxFreq, showLogScale, palette]);

    return (
        <div style={{ width: width || '100%', marginTop: 8 }}>
            <div
                style={{
                    padding: 6,
                    border: '1px solid var(--border-subtle)',
                    borderRadius: 6,
                    backgroundColor: 'var(--bg-panel)'
                }}
            >
                <canvas
                    ref={stripRef}
                    style={{
                        display: 'block',
                        width: '100%',
                        height: STRIP_HEIGHT,
                        boxSizing: 'border-box',
                        borderRadius: 4,
                        border: '1px solid var(--border-subtle)'
                    }}
                    title={`Rolling frequency score density`}
                />
            </div>
            <canvas
                ref={ticksRef}
                style={{ display: 'block', width: '100%', height: TICK_HEIGHT, marginTop: 6, userSelect: 'none' }}
            />
        </div>
    );
};

export default RollingNotesHeatMap;
