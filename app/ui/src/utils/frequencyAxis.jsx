import { MIN_FREQ } from '../constants';
import { BINARY_FRAME } from './protocol';

const TICKS = [100, 200, 300, 500, 1000, 2000, 3000, 5000];

// Log frequency, so every octave has the same width.
export const makeAxis = ({ minHz = MIN_FREQ, maxHz, width }) => {
    if (!(maxHz > minHz) || !(width > 0))
        return null;

    const minLog = Math.log10(minHz);
    const span = Math.log10(maxHz) - minLog;
    const toX = hz => (Math.log10(hz) - minLog) / span * width;
    const ticks = TICKS.filter(hz => hz > minHz && hz < maxHz);

    return { minHz, maxHz, width, toX, ticks };
};

const isLogSpectrum = spectrum => spectrum.kind === BINARY_FRAME.KIND.LOG_SPECTRUM;

export const binToHz = (spectrum, bin) =>
    isLogSpectrum(spectrum) ? spectrum.startHz * 2 ** (bin / spectrum.resolution) : bin * spectrum.resolution;

export const hzToBin = (spectrum, hz) =>
    isLogSpectrum(spectrum) ? spectrum.resolution * Math.log2(hz / spectrum.startHz) : hz / spectrum.resolution;

// The top of the spectrum's axis: the frame's own when there is one, otherwise half the
// analysis rate. The spectrum and the keyboard both take it from here, so they agree.
export const spectrumMaxHz = (spectrum, sampleRate) =>
    spectrum?.count > 0 ? binToHz(spectrum, isLogSpectrum(spectrum) ? spectrum.data.length - 1 : spectrum.data.length) : sampleRate / 2;

// 50 → '50', 1000 → '1k', 1500 → '1.5k'
export const formatHz = hz => hz >= 1000 ? `${Number((hz / 1000).toFixed(1))}k` : `${Math.round(hz)}`;
