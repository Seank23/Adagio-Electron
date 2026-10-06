import Styled from '@emotion/styled';
import { Modal } from 'antd';
import { useDispatch, useSelector } from 'react-redux';
import { RotateCcw, Settings, X } from 'lucide-react';
import AppearanceSection from './AppearanceSection';
import AnalysisSection from './AnalysisSection';
import AboutSection from './AboutSection';
import { Spacer } from '../Panel';
import { setPreferencesOpen } from '../../store/appSlice';
import { usePreferenceActions } from '../../hooks/usePreferences';

// antd's shell gives the focus trap, Esc, focus back on the cog and aria-modal; the rest is ours.
const MODAL_STYLES = {
    mask: { background: 'var(--bg-scrim)' },
    container: {
        padding: 0,
        background: 'var(--bg-panel)',
        border: '1px solid var(--border-subtle)',
        borderRadius: 10,
        boxShadow: '0 16px 48px var(--shadow-modal)',
        overflow: 'hidden',
    },
    header: {
        display: 'flex',
        alignItems: 'center',
        height: 52,
        margin: 0,
        padding: '0 12px 0 20px',
        background: 'none',
        borderBottom: '1px solid var(--border-subtle)',
    },
    title: {
        display: 'flex',
        alignItems: 'center',
        gap: 10,
        font: '600 14px/1 var(--font-ui)',
        color: 'var(--text-primary)',
    },
    body: { padding: '0 20px 8px' },
    footer: { margin: 0, padding: '12px 16px 12px 12px', borderTop: '1px solid var(--border-subtle)' },
};

// Every change applies at once, so there's no Cancel: Done, Esc, the close button and the scrim
// all just close it. destroyOnHidden unmounts the sections, and their selectors, while closed.
const PreferencesModal = () => {
    const dispatch = useDispatch();
    const open = useSelector(state => state.app.preferencesOpen);
    const close = () => dispatch(setPreferencesOpen(false));

    return (
        <StyledModal
            open={open}
            onCancel={close}
            centered
            width={560}
            destroyOnHidden
            mask={{ blur: false }}
            title={<><TitleIcon><Settings size={16} /></TitleIcon>Preferences</>}
            closeIcon={<X size={16} />}
            footer={<Footer onDone={close} />}
            styles={MODAL_STYLES}
        >
            <AppearanceSection />
            <AnalysisSection />
            <AboutSection />
        </StyledModal>
    );
};
export default PreferencesModal;

const Footer = ({ onDone }) => {
    const { restoreDefaults } = usePreferenceActions();
    return (
        <FooterRow>
            <RestoreButton type="button" onClick={restoreDefaults}>
                <RotateCcw size={14} />
                Restore defaults
            </RestoreButton>
            <Spacer />
            <DoneButton type="button" onClick={onDone}>Done</DoneButton>
        </FooterRow>
    );
};

// The className lands on .ant-modal; the extra class outranks antd's own close button rules.
const StyledModal = Styled(Modal)`
    &.ant-modal .ant-modal-close {
        top: 12px;
        inset-inline-end: 12px;
        display: inline-flex;
        align-items: center;
        justify-content: center;
        width: 28px;
        height: 28px;
        border-radius: 6px;
        color: var(--text-secondary);
        &:hover {
            background: var(--bg-surface);
            color: var(--text-primary);
        }
        &:focus-visible {
            outline: 2px solid var(--accent-primary);
            outline-offset: 1px;
        }
    }
`;

const TitleIcon = Styled.span`
    display: inline-flex;
    color: var(--text-secondary);
`;

const FooterRow = Styled.div`
    display: flex;
    align-items: center;
`;

const footerButton = `
    display: inline-flex;
    align-items: center;
    gap: 6px;
    border: none;
    border-radius: 6px;
    cursor: pointer;
    &:focus-visible {
        outline: 2px solid var(--accent-primary);
        outline-offset: 1px;
    }
`;

const RestoreButton = Styled.button`
    ${footerButton}
    padding: 7px 10px;
    background: none;
    color: var(--text-secondary);
    font: 500 12px/1.2 var(--font-ui);
    &:hover { background: var(--bg-surface); color: var(--text-primary); }
`;

const DoneButton = Styled.button`
    ${footerButton}
    padding: 7px 16px;
    background: var(--accent-primary);
    color: var(--text-on-accent);
    font: 600 12px/1.2 var(--font-ui);
    &:hover { filter: brightness(1.1); }
`;
