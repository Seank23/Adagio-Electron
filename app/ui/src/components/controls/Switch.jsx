import Styled from '@emotion/styled';

// An on/off button. antd's Switch can't take a different knob colour per state from the tokens.
const Switch = ({ label, checked, onChange, disabled = false }) => (
    <Track
        type="button"
        role="switch"
        aria-checked={checked}
        aria-label={label}
        checked={checked}
        disabled={disabled}
        onClick={() => onChange(!checked)}
    >
        <Knob checked={checked} />
    </Track>
);
export default Switch;

const withoutChecked = { shouldForwardProp: prop => prop !== 'checked' };

const Track = Styled('button', withoutChecked)`
    position: relative;
    flex-shrink: 0;
    box-sizing: border-box;
    width: 28px;
    height: 16px;
    padding: 0;
    border: 1px solid ${({ checked }) => checked ? 'var(--accent-primary)' : 'var(--border-strong)'};
    border-radius: 8px;
    background: ${({ checked }) => checked ? 'var(--accent-primary)' : 'var(--bg-raised)'};
    cursor: pointer;
    &:disabled { opacity: .5; cursor: default; }
    &:focus-visible { outline: 2px solid var(--accent-primary); outline-offset: 2px; }
`;

const Knob = Styled('span', withoutChecked)`
    position: absolute;
    top: 1px;
    left: ${({ checked }) => checked ? 13 : 1}px;
    width: 12px;
    height: 12px;
    border-radius: 50%;
    background: ${({ checked }) => checked ? 'var(--text-on-accent)' : 'var(--text-secondary)'};
    transition: left .12s ease;
    @media (prefers-reduced-motion: reduce) { transition: none; }
`;
