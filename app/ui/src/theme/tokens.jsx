// This is the only file with colour literals: antd, the CSS variables and the canvases all read from here.
export const PALETTES = {
    dark: {
        mode: 'dark',
        bg: { app: '#121212', panel: '#1B1B1B', surface: '#242424', raised: '#2F2F2F', inset: '#0B0B0B', scrim: '#00000099' },
        border: { subtle: '#2A2A2A', strong: '#3A3A3A' },
        shadow: { modal: '#00000080' },
        text: { primary: '#E8E8E8', secondary: '#9B9B9B', muted: '#5F5F5F', onAccent: '#121212' },
        accent: {
            primary: '#1D96E6', primaryFg: '#1D96E6', primaryText: '#2C9DE8', soft: '#1D96E62E',
            secondary: '#2E8B57', secondaryFg: '#2E8B57', secondaryText: '#38AA6A', secondarySoft: '#2E8B572E',
        },
        status: { ok: '#5ED17A', warning: '#E5B454', error: '#F06A6A' },
        tuning: { in: '#5ED17A', near: '#E5B454', off: '#F06A6A' },
        piano: { whiteKey: '#3A3A3A', blackKey: '#141414', gap: '#0B0B0B', whiteKeyText: '#A0A0A0', blackKeyText: '#8A8A8A' },
    },
    light: {
        mode: 'light',
        bg: { app: '#E8E8E8', panel: '#FFFFFF', surface: '#F2F2F2', raised: '#E6E6E6', inset: '#F5F5F5', scrim: '#0000004D' },
        border: { subtle: '#E3E3E3', strong: '#CFCFCF' },
        shadow: { modal: '#0000002E' },
        text: { primary: '#1C1C1C', secondary: '#555555', muted: '#767676', onAccent: '#121212' },
        accent: {
            primary: '#1D96E6', primaryFg: '#1789D4', primaryText: '#126CA7', soft: '#1D96E640',
            secondary: '#2E8B57', secondaryFg: '#2E8B57', secondaryText: '#277549', secondarySoft: '#2E8B5733',
        },
        status: { ok: '#2E9E4F', warning: '#A86B00', error: '#C73636' },
        tuning: { in: '#1F7A3D', near: '#8F5B00', off: '#C23030' },
        piano: { whiteKey: '#FFFFFF', blackKey: '#2B2B2B', gap: '#BDBDBD', whiteKeyText: '#6B6B6B', blackKeyText: '#CFCFCF' },
    },
};

export const FONTS = {
    ui: "'Inter', 'Segoe UI', sans-serif",
    mono: "'JetBrains Mono', Consolas, monospace",
};

const toKebab = name => name.replace(/[A-Z]/g, letter => `-${letter.toLowerCase()}`);

// { '--bg-panel': '#1B1B1B', '--accent-primary-fg': ..., '--font-ui': ... }
export const toCssVariables = palette => {
    const variables = {
        '--font-ui': FONTS.ui,
        '--font-mono': FONTS.mono,
    };
    Object.entries(palette).forEach(([group, values]) => {
        if (typeof values !== 'object') return;
        Object.entries(values).forEach(([name, value]) => {
            variables[`--${group}-${toKebab(name)}`] = value;
        });
    });
    return variables;
};
