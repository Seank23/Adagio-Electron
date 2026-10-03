// The waveform arrives as [{ resolution, peaks }], one set per resolution in samples per peak.

export const coarsestPeaks = waveform =>
    waveform.reduce((coarsest, set) => set.resolution > coarsest.resolution ? set : coarsest);

// One value per column: the largest |peak| in it, scaled so the loudest column is 1.
export const peakColumns = (peaks, columns) => {
    const out = new Float32Array(Math.max(0, columns));
    const perColumn = peaks.length / columns;
    let max = 0;
    for (let column = 0; column < columns; column++) {
        const start = Math.floor(column * perColumn);
        const end = Math.max(start + 1, Math.floor((column + 1) * perColumn));
        let peak = 0;
        for (let i = start; i < end && i < peaks.length; i++)
            peak = Math.max(peak, Math.abs(peaks[i]));
        out[column] = peak;
        max = Math.max(max, peak);
    }
    if (max > 0)
        for (let column = 0; column < columns; column++)
            out[column] /= max;
    return out;
};
