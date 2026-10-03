// Math.clamp is only a proposal and isn't in Electron's V8.
export const clamp = (value, min, max) => Math.min(Math.max(value, min), max);
