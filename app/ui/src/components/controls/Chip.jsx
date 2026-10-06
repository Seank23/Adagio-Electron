import Styled from '@emotion/styled';

const Button = Styled('button', { shouldForwardProp: prop => prop !== 'selected' && prop !== 'mono' })`
    display: inline-flex;
    align-items: center;
    justify-content: center;
    gap: 6px;
    padding: 4px 10px;
    border: none;
    border-radius: 5px;
    background: ${({ selected }) => selected ? 'var(--bg-raised)' : 'var(--bg-surface)'};
    color: ${({ selected }) => selected ? 'var(--accent-primary-text)' : 'var(--text-secondary)'};
    font: 500 11px/1.2 ${({ mono }) => mono ? 'var(--font-mono)' : 'var(--font-ui)'};
    white-space: nowrap;
    cursor: pointer;
    &:hover:not(:disabled) {
        color: ${({ selected }) => selected ? 'var(--accent-primary-text)' : 'var(--text-primary)'};
    }
    &:disabled {
        cursor: default;
    }
    &:focus-visible {
        outline: 2px solid var(--accent-primary);
        outline-offset: 1px;
    }
`;

// mono: number labels in JetBrains Mono, as the Preferences chips have.
const Chip = ({ selected = false, mono = false, ...rest }) => <Button selected={selected} mono={mono} aria-pressed={selected} {...rest} />;
export default Chip;
