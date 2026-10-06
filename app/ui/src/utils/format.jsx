const pad = (value, width) => String(value).padStart(width, '0');

// 84.38 → '01:24.380'
export const formatClock = seconds => {
    const totalMs = Math.max(0, Math.round((seconds || 0) * 1000));
    const minutes = Math.floor(totalMs / 60000);
    return `${pad(minutes, 2)}:${pad(Math.floor(totalMs / 1000) % 60, 2)}.${pad(totalMs % 1000, 3)}`;
};

// 312 → '5:12'
export const formatDuration = seconds => {
    const total = Math.max(0, Math.round(seconds || 0));
    return `${Math.floor(total / 60)}:${pad(total % 60, 2)}`;
};

// 'C:\music\Song.mp3' → 'Song.mp3'
export const fileNameOf = path => path.split(/[\\/]/).pop();

// 'Song.mp3' → 'MP3'
export const formatOf = name => {
    const dot = name.lastIndexOf('.');
    return dot > 0 ? name.slice(dot + 1).toUpperCase() : '';
};

// 44100 → '44.1 kHz', 48000 → '48 kHz'
export const formatSampleRate = hz => hz ? `${Number((hz / 1000).toFixed(1))} kHz` : '';

const oneDecimal = value => Number(value.toFixed(1));

// 8000 → 'up to 4 kHz'
export const formatAnalysisRange = sampleRate => `up to ${oneDecimal(sampleRate / 2000)} kHz`;

// 64 at 8000 → '8 ms · 125 frames/s'
export const formatHop = (hopSize, sampleRate) =>
    `${oneDecimal(hopSize / sampleRate * 1000)} ms · ${oneDecimal(sampleRate / hopSize)} frames/s`;

// 4096 at 8000 → '1.95 Hz per bin · 512 ms'
export const formatFrame = (frameLength, sampleRate) =>
    `${(sampleRate / frameLength).toFixed(2)} Hz per bin · ${oneDecimal(frameLength / sampleRate * 1000)} ms`;

// 4 of 128 at 8000 → 'last 64 ms'; 1 → 'off'
export const formatSmoothing = (count, hopSize, sampleRate) =>
    count > 1 ? `last ${oneDecimal(count * hopSize / sampleRate * 1000)} ms` : 'off';

// 4 of 128 at 8000 → '4-frame smoothing · 64 ms'; 1 → 'no smoothing'
export const formatSmoothingSummary = (count, hopSize, sampleRate) =>
    count > 1 ? `${count}-frame smoothing · ${oneDecimal(count * hopSize / sampleRate * 1000)} ms` : 'no smoothing';

export const formatChannels = channels =>
    channels === 1 ? 'mono' : channels === 2 ? 'stereo' : channels ? `${channels} ch` : '';
