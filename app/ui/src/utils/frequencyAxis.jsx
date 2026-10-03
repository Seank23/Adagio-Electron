import { MIN_FREQ } from '../constants';

const LOG_TICKS = [100, 200, 300, 500, 1000, 2000, 3000];
const LINEAR_TICK_STEP = 500;

export const makeAxis = ({ minHz = MIN_FREQ, maxHz, width, log }) => {
    if (!(maxHz > minHz) || !(width > 0))
        return null;

    const toX = log
        ? (() => {
            const minLog = Math.log10(minHz);
            const span = Math.log10(maxHz) - minLog;
            return hz => (Math.log10(hz) - minLog) / span * width;
        })()
        : hz => (hz - minHz) / (maxHz - minHz) * width;

    const ticks = log
        ? LOG_TICKS.filter(hz => hz > minHz && hz < maxHz)
        : Array.from({ length: Math.floor(maxHz / LINEAR_TICK_STEP) }, (_, i) => (i + 1) * LINEAR_TICK_STEP)
            .filter(hz => hz > minHz && hz < maxHz);

    return { minHz, maxHz, width, log, toX, ticks };
};

// 50 → '50', 1000 → '1k', 1500 → '1.5k'
export const formatHz = hz => hz >= 1000 ? `${Number((hz / 1000).toFixed(1))}k` : `${Math.round(hz)}`;
