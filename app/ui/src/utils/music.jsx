export const A4_DEFAULT_HZ = 440;

export const centsFrom440 = hz => 1200 * Math.log2(hz / A4_DEFAULT_HZ);

export const tuningBand = cents => {
    const distance = Math.abs(cents);
    return distance <= 10 ? 'in' : distance <= 20 ? 'near' : 'off';
};

export const formatCents = (cents, decimals = 0) => {
    const rounded = Number(cents.toFixed(decimals));
    const sign = rounded > 0 ? '+' : rounded < 0 ? '−' : '';
    return `${sign}${Math.abs(rounded).toFixed(decimals)}¢`;
};

// The engine's NoteNames: flats only, indexed by pitch class starting on C.
export const NOTE_NAMES = ['C', 'Db', 'D', 'Eb', 'E', 'F', 'Gb', 'G', 'Ab', 'A', 'Bb', 'B'];

// Folded against the reference the engine names notes with, never a fixed 440.
export const midiOf = (hz, a4) => Math.round(69 + 12 * Math.log2(hz / a4));

