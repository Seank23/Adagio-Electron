// Holds what arrives in binary frames: the latest spectrum and the waveform set.

const createSignal = () => {
    const listeners = new Set();
    return {
        subscribe: listener => {
            listeners.add(listener);
            return () => listeners.delete(listener);
        },
        notify: () => listeners.forEach(listener => listener()),
    };
};

let latestSpectrum = null;
const spectrumSignal = createSignal();

// Arrival times of the spectrum frames shown in the last second, for the status bar.
const RATE_WINDOW_MS = 1000;
const spectrumTimes = [];
const pruneSpectrumTimes = now => {
    while (spectrumTimes.length > 0 && spectrumTimes[0] < now - RATE_WINDOW_MS) spectrumTimes.shift();
};

export const subscribeSpectrum = spectrumSignal.subscribe;
export const getLatestSpectrum = () => latestSpectrum;

export const publishSpectrum = frame => {
    if (latestSpectrum && frame.seekGeneration < latestSpectrum.seekGeneration) return;
    latestSpectrum = frame;
    const now = performance.now();
    spectrumTimes.push(now);
    pruneSpectrumTimes(now);
    spectrumSignal.notify();
};

export const getSpectrumRate = () => {
    pruneSpectrumTimes(performance.now());
    return spectrumTimes.length * 1000 / RATE_WINDOW_MS;
};

export const resetSpectrum = () => {
    latestSpectrum = null;
    spectrumSignal.notify();
};

// Waveform frames arrive one per resolution, ahead of fileLoaded. They are staged
// and published together on fileLoaded, so a reader never sees a half-replaced set.
let waveform = null;
let stagedWaveform = [];
const waveformSignal = createSignal();

export const subscribeWaveform = waveformSignal.subscribe;
export const getWaveform = () => waveform;

export const stageWaveform = frame => {
    stagedWaveform.push({ resolution: frame.resolution, peaks: frame.data });
};

export const commitWaveform = () => {
    if (stagedWaveform.length === 0) return;
    waveform = stagedWaveform;
    stagedWaveform = [];
    waveformSignal.notify();
};

export const clearFrames = () => {
    stagedWaveform = [];
    waveform = null;
    latestSpectrum = null;
    waveformSignal.notify();
    spectrumSignal.notify();
};
