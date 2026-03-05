export const EVENT_TYPE = {
    END_OF_PLAY: 'endOfPlay',
    POSITION: 'position',
    FILE_LOADED: 'fileLoaded',
    FILE_CLOSED: 'fileClosed',
    SUCCESS: 'success',
    ERROR: 'error',
    INFO: 'info',
    WAVEFORM_DATA: 'waveformData',
    ANALYSIS: 'analysis',
};

export const clamp01 = value => Math.max(0, Math.min(1, value));

export const parseHexColor = color => {
    const cleaned = color.replace('#', '').trim();

    if (cleaned.length === 3) {
        return [
            parseInt(cleaned[0] + cleaned[0], 16),
            parseInt(cleaned[1] + cleaned[1], 16),
            parseInt(cleaned[2] + cleaned[2], 16)
        ];
    }

    if (cleaned.length === 6) {
        return [
            parseInt(cleaned.slice(0, 2), 16),
            parseInt(cleaned.slice(2, 4), 16),
            parseInt(cleaned.slice(4, 6), 16)
        ];
    }

    return null;
};

export const parseRgbColor = color => {
    const rgbMatches = color.match(/^rgba?\(([^)]+)\)$/i);

    if (!rgbMatches) {
        return null;
    }

    const channels = rgbMatches[1]
        .split(',')
        .slice(0, 3)
        .map(part => Number(part.trim()));

    if (channels.length !== 3 || channels.some(channel => Number.isNaN(channel))) {
        return null;
    }

    return channels.map(channel => Math.max(0, Math.min(255, Math.round(channel))));
};

export const toRgbChannels = color => {
    if (typeof color !== 'string') {
        return [0, 0, 0];
    }

    if (color.startsWith('#')) {
        const parsedHex = parseHexColor(color);

        if (parsedHex) {
            return parsedHex;
        }
    }

    const parsedRgb = parseRgbColor(color);

    if (parsedRgb) {
        return parsedRgb;
    }

    return [0, 0, 0];
};