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

export const formatChannels = channels =>
    channels === 1 ? 'mono' : channels === 2 ? 'stereo' : channels ? `${channels} ch` : '';
