// Holds what arrives in binary frames: the latest spectrum and the waveform set.
// Typed arrays at up to 125 Hz don't belong in Redux, whose dev-mode checks would
// walk every element on every action. Each half has subscribe/getSnapshot for
// useSyncExternalStore; EngineEventRouter is the only writer.

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
    latestSpectrum = frame;
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
