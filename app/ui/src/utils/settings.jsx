// The rules the Settings tab's controls follow, kept out of the components so the tests reach them.
// A setting is one entry of a stage in getAnalysisSchema's reply: { key, name, type, min?, max?, options?, default, value }.
import { clamp } from './math';

const SEGMENTED_MAX_LABEL = 12;
const MAX_DECIMALS = 4;

const isNumberType = type => type === 'int' || type === 'float';

// Stored floats come back widened (0.2f is 0.20000000298…), so numbers compare within a millionth.
const nearlyEqual = (a, b) => Math.abs(a - b) <= 1e-6 * Math.max(1, Math.abs(b));

const roundTo = (value, decimals) => Number(value.toFixed(decimals));

// 'number' | 'switch' | 'segmented' | 'select'
export const controlKind = setting => {
    if (isNumberType(setting.type))
        return 'number';
    const options = setting.options ?? [];
    if (options.length === 2 && options.includes('Yes') && options.includes('No'))
        return 'switch';
    if (options.length === 2 && options.every(option => option.length <= SEGMENTED_MAX_LABEL))
        return 'segmented';
    return 'select';
};

export const isChanged = setting => {
    if (setting.default === undefined)
        return false;
    if (typeof setting.value === 'number' && typeof setting.default === 'number')
        return !nearlyEqual(setting.value, setting.default);
    return setting.value !== setting.default;
};

export const stageHasChanges = stage => stage.settings.some(isChanged);

// int: 1. float: the largest 1, 2 or 5 × 10ⁿ at or under a hundredth of the range.
// A4 (415–466) 0.5, the chord window (0.01–5) 0.02, the presence window (0–1) 0.01, the cut-offs (0–20000) 200.
export const stepOf = setting => {
    if (setting.type === 'int')
        return 1;
    const target = ((setting.max ?? 1) - (setting.min ?? 0)) / 100;
    if (!(target > 0))
        return 0.01;
    const power = 10 ** Math.floor(Math.log10(target) + 1e-9);
    const mantissa = [5, 2, 1].find(m => m * power <= target * (1 + 1e-9));
    return Number((mantissa * power).toPrecision(1));
};

const decimalsOf = step => Math.max(0, Math.ceil(-Math.log10(step) - 1e-9));

// A number with its step's decimals (442.0, 0.50, 55), or more when it sits off that grid (a typed 0.001).
export const formatSetting = (setting, value) => {
    if (typeof value !== 'number')
        return value === undefined || value === null ? '—' : String(value);
    let decimals = decimalsOf(stepOf(setting));
    while (decimals < MAX_DECIMALS && !nearlyEqual(roundTo(value, decimals), value))
        decimals++;
    return value.toFixed(decimals);
};

// The slider's ends on its step's grid: antd counts steps from min, so 0.01 with a step of 0.02 would put
// 0.5 between two stops. A value the field takes outside them sits at the slider's end.
export const sliderRange = setting => {
    const step = stepOf(setting);
    const decimals = decimalsOf(step);
    return {
        min: roundTo(Math.ceil(setting.min / step - 1e-9) * step, decimals),
        max: roundTo(Math.floor(setting.max / step + 1e-9) * step, decimals),
        step,
    };
};

// What a value sent for the setting looks like: an integer for int, and a float without the noise of
// stepping by 0.1 (0.30000000000000004), clamped to the range.
export const normaliseSetting = (setting, value) => {
    const rounded = setting.type === 'int' ? Math.round(value) : Number(value.toPrecision(12));
    return clamp(rounded, setting.min ?? -Infinity, setting.max ?? Infinity);
};

// Typed text → a value, or null for nothing a number.
export const parseSetting = (setting, text) => {
    const value = Number.parseFloat(String(text).trim());
    return Number.isFinite(value) ? normaliseSetting(setting, value) : null;
};

// Drops a trailing "Processor", then splits the words, keeping acronyms whole:
// "HPSDownsamplerProcessor" → "HPS Downsampler", "FFTProcessor" → "FFT".
export const formatStageName = name => name
    .replace(/Processor$/, '')
    .replace(/([a-z0-9])([A-Z])/g, '$1 $2')
    .replace(/([A-Z]+)([A-Z][a-z])/g, '$1 $2');
