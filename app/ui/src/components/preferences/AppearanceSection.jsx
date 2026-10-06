import { useSelector } from 'react-redux';
import { Monitor, Moon, Sun } from 'lucide-react';
import Segmented from '../controls/Segmented';
import { PreferenceRow, PreferenceSection } from './PreferenceRow';
import { usePreferenceActions } from '../../hooks/usePreferences';

const THEMES = [
    { value: 'system', label: 'System', icon: <Monitor size={14} /> },
    { value: 'dark', label: 'Dark', icon: <Moon size={14} /> },
    { value: 'light', label: 'Light', icon: <Sun size={14} /> },
];

const AppearanceSection = () => {
    const { setTheme } = usePreferenceActions();
    const themeMode = useSelector(state => state.settings.themeMode);

    return (
        <PreferenceSection title="Appearance">
            <PreferenceRow label="Theme" description="Follows the system unless you pick one.">
                <Segmented label="Theme" options={THEMES} value={themeMode} onChange={setTheme} />
            </PreferenceRow>
        </PreferenceSection>
    );
};
export default AppearanceSection;
