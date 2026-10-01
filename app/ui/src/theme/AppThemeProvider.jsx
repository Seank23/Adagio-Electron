import { useLayoutEffect, useMemo, useSyncExternalStore } from 'react';
import { useSelector } from 'react-redux';
import { ConfigProvider, theme } from 'antd';
import { PALETTES, FONTS, toCssVariables } from './tokens';
import { PaletteContext } from './PaletteContext';

// Electron's renderer follows the Windows app theme through this query.
const darkQuery = window.matchMedia('(prefers-color-scheme: dark)');
const subscribeSystemDark = callback => {
    darkQuery.addEventListener('change', callback);
    return () => darkQuery.removeEventListener('change', callback);
};
const getSystemDark = () => darkQuery.matches;

const buildAntdTheme = palette => ({
    algorithm: palette.mode === 'dark' ? theme.darkAlgorithm : theme.defaultAlgorithm,
    token: {
        colorPrimary: palette.accent.primary,
        colorBgBase: palette.bg.app,
        colorBgLayout: palette.bg.app,
        colorBgContainer: palette.bg.panel,
        colorBgElevated: palette.bg.raised,
        colorBorder: palette.border.strong,
        colorBorderSecondary: palette.border.subtle,
        colorText: palette.text.primary,
        colorTextSecondary: palette.text.secondary,
        colorTextTertiary: palette.text.muted,
        colorSuccess: palette.status.ok,
        colorWarning: palette.status.warning,
        colorError: palette.status.error,
        fontFamily: FONTS.ui,
        fontFamilyCode: FONTS.mono,
        fontSize: 12,
        borderRadius: 6,
    },
    components: {
        Slider: {
            railBg: palette.bg.inset,
            railHoverBg: palette.bg.inset,
            trackBg: palette.accent.primary,
            trackHoverBg: palette.accent.primary,
            handleColor: palette.text.primary,
            handleActiveColor: palette.text.primary,
            railSize: 4,
            handleSize: 12,
            handleSizeHover: 12,
        },
    },
});

// Publishes one palette three ways: antd tokens through ConfigProvider, CSS custom
// properties on <html> for styled components, and PaletteContext for canvases.
const AppThemeProvider = ({ children }) => {
    const themeMode = useSelector(state => state.settings.themeMode);
    const systemDark = useSyncExternalStore(subscribeSystemDark, getSystemDark);
    const mode = themeMode === 'system' ? (systemDark ? 'dark' : 'light') : themeMode;
    const palette = PALETTES[mode] ?? PALETTES.light;

    const antdTheme = useMemo(() => buildAntdTheme(palette), [palette]);

    useLayoutEffect(() => {
        const root = document.documentElement;
        Object.entries(toCssVariables(palette)).forEach(([name, value]) => {
            root.style.setProperty(name, value);
        });
        root.dataset.theme = palette.mode;
        root.style.colorScheme = palette.mode;
        root.style.backgroundColor = 'var(--bg-app)';
        root.style.color = 'var(--text-primary)';
    }, [palette]);

    return (
        <ConfigProvider theme={antdTheme}>
            <PaletteContext.Provider value={palette}>
                {children}
            </PaletteContext.Provider>
        </ConfigProvider>
    );
};
export default AppThemeProvider;
