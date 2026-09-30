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

export const subscribeSpectrum = spectrumSignal.subscribe;
export const getLatestSpectrum = () => latestSpectrum;

export const publishSpectrum = frame => {
    if (latestSpectrum && frame.seekGeneration < latestSpectrum.seekGeneration) return;
    latestSpectrum = frame;
    spectrumSignal.notify();
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
