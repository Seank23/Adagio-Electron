import Styled from '@emotion/styled';

const Button = Styled('button', { shouldForwardProp: prop => prop !== 'selected' && prop !== 'font' })`
    display: inline-flex;
    align-items: center;
    justify-content: center;
    gap: 6px;
    padding: 4px 10px;
    border: none;
    border-radius: 5px;
    background: ${({ selected }) => selected ? 'var(--bg-raised)' : 'var(--bg-surface)'};
    color: ${({ selected }) => selected ? 'var(--accent-primary-text)' : 'var(--text-secondary)'};
    font: 500 11px/1.2 ${({ font }) => `var(${font})`};
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

const Chip = ({ selected = false, font = '--font-ui', ...rest }) => <Button selected={selected} font={font} aria-pressed={selected} {...rest} />;
export default Chip;
