import React, { useMemo } from 'react';
import { theme } from 'antd';
import { useSelector } from 'react-redux';
import { clamp01, toRgbChannels } from '../utils/utils';

const DEFAULT_MIN_FREQ = 50;
const DEFAULT_MAX_FREQ = 22050;
const DENSITY_SMOOTHING = 0.001;
const MAX_RENDER_SAMPLES = 1024;
const MAX_GRADIENT_STOPS = 720;
const HOTSPOT_GAIN = 4.0;
const HOTSPOT_EXPONENT = 0.8;
const NOTE_NAMES = ['C', 'Db', 'D', 'Eb', 'E', 'F', 'Gb', 'G', 'Ab', 'A', 'Bb', 'B'];

const getHeatColor = (intensity, highColor) => {
    const t = clamp01(intensity);
    const [targetRed, targetGreen, targetBlue] = toRgbChannels(highColor);
    const red = Math.round(255 + ((targetRed - 255) * t));
    const green = Math.round(255 + ((targetGreen - 255) * t));
    const blue = Math.round(255 + ((targetBlue - 255) * t));
    return `rgb(${red}, ${green}, ${blue})`;
};

const RollingNotesHeatMap = ({ width, minFreq = DEFAULT_MIN_FREQ, maxFreq = DEFAULT_MAX_FREQ, showLogScale = false }) => {
    const { token } = theme.useToken();
    const primaryHeatColor = token.colorPrimary;
    const noteScores = useSelector(state => state.analysis.frequencyHistogram);

    const { samples, maxScore, safeMinFreq, safeMaxFreq } = useMemo(() => {
        const safeMin = Number.isFinite(minFreq) ? minFreq : DEFAULT_MIN_FREQ;
        const safeMax = Number.isFinite(maxFreq) && maxFreq > safeMin ? maxFreq : DEFAULT_MAX_FREQ;
        const span = Math.max(1e-6, safeMax - safeMin);
        const logMin = Math.log10(safeMin);
        const logMax = Math.log10(safeMax);
        const logSpan = Math.max(1e-6, logMax - logMin);
        const targetWidth = Number.isFinite(width) ? width : 1000;
        const sampleCount = Math.max(120, Math.min(MAX_RENDER_SAMPLES, Math.round(targetWidth * 0.85)));
        const scoreSamples = Array.from({ length: sampleCount }, () => 0);
        const sampleMaxIndex = Math.max(1, sampleCount - 1);
        const sigmaInSamples = Math.max(1, DENSITY_SMOOTHING * sampleMaxIndex);
        const activeRadius = Math.max(1, Math.ceil(sigmaInSamples * 3));
        const twoSigmaSquared = 2 * sigmaInSamples * sigmaInSamples;

        noteScores.forEach(entry => {
            if (entry.frequency < safeMin || entry.frequency > safeMax) {
                return;
            }

            const normalized = showLogScale
                ? (Math.log10(entry.frequency) - logMin) / logSpan
                : (entry.frequency - safeMin) / span;
            const clamped = clamp01(normalized);
            const centerIndex = Math.round(clamped * sampleMaxIndex);
            const startIndex = Math.max(0, centerIndex - activeRadius);
            const endIndex = Math.min(sampleMaxIndex, centerIndex + activeRadius);

            for (let index = startIndex; index <= endIndex; index += 1) {
                const delta = index - centerIndex;
                const weight = Math.exp(-(delta * delta) / twoSigmaSquared);
                scoreSamples[index] += entry.score * weight;
            }
        });

        const highestScore = scoreSamples.reduce((max, score) => (score > max ? score : max), 0);

        return {
            samples: scoreSamples,
            maxScore: highestScore,
            safeMinFreq: safeMin,
            safeMaxFreq: safeMax
        };
    }, [noteScores, minFreq, maxFreq, width, showLogScale]);

    const heatGradient = useMemo(() => {

        if (!samples.length || maxScore <= 0) {
            return `linear-gradient(to right, ${getHeatColor(0, primaryHeatColor)} 0%, ${getHeatColor(0, primaryHeatColor)} 100%)`;
        }

        const stopStep = Math.max(1, Math.floor(samples.length / MAX_GRADIENT_STOPS));
        const stops = [];

        for (let index = 0; index < samples.length; index += stopStep) {
            const score = samples[index];
            const normalizedScore = score / maxScore;
            const boostedIntensity = clamp01(Math.pow(normalizedScore * HOTSPOT_GAIN, HOTSPOT_EXPONENT));
            const position = (index / Math.max(1, samples.length - 1)) * 100;
            stops.push(`${getHeatColor(boostedIntensity, primaryHeatColor)} ${position.toFixed(3)}%`);
        }

        if ((samples.length - 1) % stopStep !== 0) {
            const lastIndex = samples.length - 1;
            const lastNormalized = samples[lastIndex] / maxScore;
            const lastIntensity = clamp01(Math.pow(lastNormalized * HOTSPOT_GAIN, HOTSPOT_EXPONENT));
            stops.push(`${getHeatColor(lastIntensity, primaryHeatColor)} 100%`);
        }

        return `linear-gradient(to right, ${stops.join(', ')})`;
    }, [samples, maxScore, primaryHeatColor]);

    const noteTicks = useMemo(() => {
        if (!Number.isFinite(safeMinFreq) || !Number.isFinite(safeMaxFreq) || safeMaxFreq <= safeMinFreq) {
            return [];
        }

        const lowerMidi = Math.max(0, Math.floor(69 + (12 * Math.log2(safeMinFreq / 440))));
        const upperMidi = Math.min(127, Math.ceil(69 + (12 * Math.log2(safeMaxFreq / 440))));

        if (!Number.isFinite(lowerMidi) || !Number.isFinite(upperMidi) || lowerMidi > upperMidi) {
            return [];
        }

        const rangeLinear = Math.max(1e-6, safeMaxFreq - safeMinFreq);
        const rangeLog = Math.max(1e-6, Math.log10(safeMaxFreq) - Math.log10(safeMinFreq));
        const widthPx = Number.isFinite(width) ? width : 1000;
        const minLabelSpacingPx = 0;
        const ticks = [];
        let lastAcceptedX = -Infinity;
        const sampleCount = samples.length || 1;
        const sampleMaxIndex = Math.max(1, sampleCount - 1);

        for (let midi = lowerMidi; midi <= upperMidi; midi += 1) {
            const frequency = 440 * Math.pow(2, (midi - 69) / 12);

            if (frequency < safeMinFreq || frequency > safeMaxFreq) {
                continue;
            }

            const normalized = showLogScale
                ? (Math.log10(frequency) - Math.log10(safeMinFreq)) / rangeLog
                : (frequency - safeMinFreq) / rangeLinear;
            const clamped = clamp01(normalized);
            const x = clamped * widthPx;

            if (x - lastAcceptedX < minLabelSpacingPx) {
                continue;
            }

            // sample intensity at this frequency
            const sampleIndex = Math.round(clamped * sampleMaxIndex);
            const sampleValue = (samples && samples[sampleIndex]) ? samples[sampleIndex] : 0;
            const normalizedScore = maxScore > 0 ? (sampleValue / maxScore) : 0;
            const boostedIntensity = clamp01(Math.pow(normalizedScore * HOTSPOT_GAIN, HOTSPOT_EXPONENT));
            const labelColor = getHeatColor(boostedIntensity, primaryHeatColor);

            const noteName = NOTE_NAMES[midi % 12];
            const octave = Math.floor(midi / 12) - 1;
            ticks.push({
                key: `${noteName}${octave}`,
                positionPct: clamped * 100,
                label: `${noteName}${octave}`,
                color: labelColor,
                intensity: boostedIntensity
            });
            lastAcceptedX = x;
        }

        return ticks;
    }, [safeMinFreq, safeMaxFreq, showLogScale, width, samples, maxScore, primaryHeatColor]);

    return (
        <div style={{ width: width || '100%', marginTop: 8 }}>
            <div
                style={{
                    padding: 6,
                    border: `1px solid ${token.colorBorderSecondary}`,
                    borderRadius: token.borderRadius,
                    backgroundColor: token.colorBgContainer
                }}
            >
                <div
                    style={{
                        minHeight: 42,
                        borderRadius: Math.max(2, token.borderRadiusSM || 2),
                        backgroundImage: heatGradient,
                        boxShadow: `inset 0 0 0 1px ${token.colorBorderSecondary}`
                    }}
                    title={`Rolling frequency score density`}
                />
            </div>
            <div
                style={{
                    marginTop: 6,
                    height: 16,
                    position: 'relative',
                    fontSize: 11,
                    color: token.colorTextSecondary,
                    userSelect: 'none'
                }}
            >
                {noteTicks.map(tick => (
                    <span
                        key={tick.key}
                        style={{
                            position: 'absolute',
                            left: `${tick.positionPct}%`,
                            transform: 'translateX(-50%)',
                            whiteSpace: 'nowrap',
                            color: tick.color || token.colorTextSecondary
                        }}
                    >
                        {tick.label}
                    </span>
                ))}
            </div>
        </div>
    );
};

export default RollingNotesHeatMap;