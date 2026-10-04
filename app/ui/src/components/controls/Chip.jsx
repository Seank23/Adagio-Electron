import Styled from '@emotion/styled';

const Button = Styled('button', { shouldForwardProp: prop => prop !== 'selected' })`
    padding: 4px 10px;
    border: none;
    border-radius: 5px;
    background: ${({ selected }) => selected ? 'var(--bg-raised)' : 'var(--bg-surface)'};
    color: ${({ selected }) => selected ? 'var(--accent-primary-text)' : 'var(--text-secondary)'};
    font: 500 11px/1.2 var(--font-ui);
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

const Chip = ({ selected = false, ...rest }) => <Button selected={selected} aria-pressed={selected} {...rest} />;
export default Chip;
