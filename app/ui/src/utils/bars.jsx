// The share of the track the largest value in a list fills, leaving it a margin.
const TOP_FILL = 90;

export const barWidths = values => {
    const max = Math.max(0, ...values);
    return values.map(value => max > 0 ? `${(value / max) * TOP_FILL}%` : '0%');
};

export const formatPercent = value => `${(value ?? 0).toFixed(1)}%`;
