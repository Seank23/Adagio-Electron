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
