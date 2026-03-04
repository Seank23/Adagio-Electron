import React, { useEffect, useMemo, useRef, useState } from 'react';
import { theme } from 'antd';

const ROLLING_WINDOW_MS = 5000;
const CLEANUP_INTERVAL_MS = 200;
const DEFAULT_MIN_FREQ = 50;
const DEFAULT_MAX_FREQ = 22050;
const DENSITY_SMOOTHING = 0.002;
const MAX_RENDER_SAMPLES = 720;
const MAX_GRADIENT_STOPS = 360;
const HOTSPOT_GAIN = 1.8;
const HOTSPOT_EXPONENT = 0.45;

const clamp01 = value => Math.max(0, Math.min(1, value));

const getHeatColor = intensity => {
    const t = clamp01(intensity);
    const red = Math.round(20 + (235 * t));
    const green = Math.round(40 + (40 * (1 - t)));
    const blue = Math.round(255 - (245 * t));
    return `rgb(${red}, ${green}, ${blue})`;
};

const RollingNotesHeatMap = ({ notes, width, minFreq = DEFAULT_MIN_FREQ, maxFreq = DEFAULT_MAX_FREQ, showLogScale = false }) => {
    const { token } = theme.useToken();
    const [history, setHistory] = useState([]);
    const pendingEntriesRef = useRef([]);

    useEffect(() => {
        const now = Date.now();
        const nextPendingEntries = [];

        (notes || []).forEach(note => {
            const parsedFrequency = Number(note?.frequency);
            const parsedScore = Number(note?.score);

            if (!Number.isFinite(parsedFrequency) || !Number.isFinite(parsedScore) || parsedScore <= 0) {
                return;
            }

            nextPendingEntries.push({
                frequency: parsedFrequency,
                score: parsedScore,
                timestamp: now
            });
        });

        if (nextPendingEntries.length > 0) {
            pendingEntriesRef.current = pendingEntriesRef.current.concat(nextPendingEntries);
        }
    }, [notes]);

    useEffect(() => {
        const interval = setInterval(() => {
            const cutoff = Date.now() - ROLLING_WINDOW_MS;

            setHistory(prevHistory => {
                const prunedHistory = prevHistory.filter(entry => entry.timestamp >= cutoff);
                const pendingEntries = pendingEntriesRef.current;

                if (pendingEntries.length > 0) {
                    pendingEntriesRef.current = [];
                }

                if (pendingEntries.length === 0) {
                    return prunedHistory.length === prevHistory.length ? prevHistory : prunedHistory;
                }

                return prunedHistory.concat(pendingEntries);
            });
        }, CLEANUP_INTERVAL_MS);

        return () => clearInterval(interval);
    }, []);

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
        const activeRadius = Math.max(2, Math.ceil(sigmaInSamples * 3));
        const twoSigmaSquared = 2 * sigmaInSamples * sigmaInSamples;

        history.forEach(entry => {
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
    }, [history, minFreq, maxFreq, width, showLogScale]);

    const heatGradient = useMemo(() => {
        if (!samples.length || maxScore <= 0) {
            return `linear-gradient(to right, ${getHeatColor(0)} 0%, ${getHeatColor(0)} 100%)`;
        }

        const stopStep = Math.max(1, Math.floor(samples.length / MAX_GRADIENT_STOPS));
        const stops = [];

        for (let index = 0; index < samples.length; index += stopStep) {
            const score = samples[index];
            const normalizedScore = score / maxScore;
            const boostedIntensity = clamp01(Math.pow(normalizedScore * HOTSPOT_GAIN, HOTSPOT_EXPONENT));
            const position = (index / Math.max(1, samples.length - 1)) * 100;
            stops.push(`${getHeatColor(boostedIntensity)} ${position.toFixed(3)}%`);
        }

        if ((samples.length - 1) % stopStep !== 0) {
            const lastIndex = samples.length - 1;
            const lastNormalized = samples[lastIndex] / maxScore;
            const lastIntensity = clamp01(Math.pow(lastNormalized * HOTSPOT_GAIN, HOTSPOT_EXPONENT));
            stops.push(`${getHeatColor(lastIntensity)} 100%`);
        }

        return `linear-gradient(to right, ${stops.join(', ')})`;
    }, [samples, maxScore]);

    const formatFreqLabel = (value) => {
        if (value >= 1000) {
            return `${(value / 1000).toFixed(value >= 10000 ? 0 : 1)}k`;
        }

        return `${Math.round(value)}`;
    };

    const midFreq = showLogScale
        ? Math.pow(10, (Math.log10(safeMinFreq) + Math.log10(safeMaxFreq)) / 2)
        : (safeMinFreq + safeMaxFreq) / 2;

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
                    title={`Rolling frequency score density (${formatFreqLabel(safeMinFreq)}Hz - ${formatFreqLabel(safeMaxFreq)}Hz)`}
                />
            </div>
            <div
                style={{
                    marginTop: 4,
                    display: 'flex',
                    justifyContent: 'space-between',
                    fontSize: 11,
                    color: token.colorTextSecondary
                }}
            >
                <span>{formatFreqLabel(safeMinFreq)} Hz</span>
                <span>{formatFreqLabel(midFreq)} Hz</span>
                <span>{formatFreqLabel(safeMaxFreq)} Hz</span>
            </div>
        </div>
    );
};

export default RollingNotesHeatMap;