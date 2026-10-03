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
