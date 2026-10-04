import Styled from '@emotion/styled';
import { Tooltip } from 'antd';

// variant: 'ghost' | 'default' | 'active' | 'primary'.
// The label is the tooltip and the accessible name, since the button shows only an icon.
const IconButton = ({ icon, label, variant = 'ghost', disabled = false, onClick, tooltipProps = {}, ...rest }) => {
    const button = (
        <Button
            type="button"
            variant={variant}
            disabled={disabled}
            onClick={onClick}
            aria-label={label}
            aria-pressed={variant === 'active' ? true : undefined}
            {...rest}
        >
            {icon}
        </Button>
    );
    return label
        ? <Tooltip title={label} mouseEnterDelay={0.5} {...tooltipProps}>{disabled ? <span>{button}</span> : button}</Tooltip>
        : button;
};
export default IconButton;

const VARIANTS = {
    ghost: `
        background: none;
        color: var(--text-secondary);
        &:hover:not(:disabled) { background: var(--bg-surface); color: var(--text-primary); }
    `,
    default: `
        background: var(--bg-raised);
        color: var(--text-secondary);
        &:hover:not(:disabled) { color: var(--text-primary); }
    `,
    active: `
        background: var(--accent-soft);
        border-color: var(--accent-primary);
        color: var(--accent-primary-fg);
    `,
    primary: `
        width: 36px;
        height: 36px;
        border-radius: 18px;
        background: var(--accent-primary);
        color: var(--text-on-accent);
        &:hover:not(:disabled) { filter: brightness(1.1); }
    `,
};

const Button = Styled.button`
    display: inline-flex;
    align-items: center;
    justify-content: center;
    flex-shrink: 0;
    width: 28px;
    height: 28px;
    padding: 0;
    border: 1px solid transparent;
    border-radius: 6px;
    cursor: pointer;
    &:disabled {
        opacity: .35;
        cursor: default;
    }
    &:focus-visible {
        outline: 2px solid var(--accent-primary);
        outline-offset: 1px;
    }
    ${({ variant }) => VARIANTS[variant] ?? VARIANTS.ghost}
`;
