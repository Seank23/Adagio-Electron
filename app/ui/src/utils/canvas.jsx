// Sizes the backing store to the element at the screen's pixel ratio, which also
// clears it. Returns the ratio so drawing can stay in CSS pixels.
export const fitToElement = canvas => {
    const ratio = window.devicePixelRatio || 1;
    const width = Math.max(1, Math.round(canvas.clientWidth * ratio));
    const height = Math.max(1, Math.round(canvas.clientHeight * ratio));
    if (canvas.width !== width || canvas.height !== height) {
        canvas.width = width;
        canvas.height = height;
    }
    return ratio;
};

// '#1D96E6', .55 → '#1D96E68C'. Canvas and wavesurfer take 8-digit hex.
export const withAlpha = (hex, alpha) =>
    `${hex.slice(0, 7)}${Math.round(Math.min(Math.max(alpha, 0), 1) * 255).toString(16).padStart(2, '0')}`;
