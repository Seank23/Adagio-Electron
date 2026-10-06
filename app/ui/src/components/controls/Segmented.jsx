import Styled from '@emotion/styled';
import Chip from './Chip';

// A row of chips with one selected. options: [{ value, label, icon? }]. Clicking the selected chip sends nothing.
const Segmented = ({ label, options, value, onChange, font = '--font-ui', disabled = false }) => (
    <Track role="group" aria-label={label} aria-disabled={disabled || undefined}>
        {options.map(option => (
            <Chip
                key={option.value}
                type="button"
                font={font}
                selected={option.value === value}
                disabled={disabled}
                onClick={() => option.value !== value && onChange(option.value)}
            >
                {option.icon}
                {option.label}
            </Chip>
        ))}
    </Track>
);
export default Segmented;

// Every chip as wide as the widest.
const Track = Styled.div`
    display: inline-grid;
    grid-auto-flow: column;
    grid-auto-columns: 1fr;
    gap: 2px;
    padding: 2px;
    border-radius: 6px;
    background: var(--bg-surface);
    &[aria-disabled='true'] { opacity: .5; }
`;
