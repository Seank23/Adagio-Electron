import { useState } from 'react';
import Styled from '@emotion/styled';
import { clamp } from '../../utils/math';

// A typed number with its unit. The text is held while editing; Enter, or leaving the field, sends it
// once, Esc drops it, and ↑/↓ step by `step` (×10 with Shift). format shows a value, parse reads typed
// text back (null for nothing a number). It shows `value`, so the owner decides when a change lands.
const NumberField = ({ label, value, unit, step, min, max, format, parse, onCommit, changed = false, disabled = false }) => {
    const [draft, setDraft] = useState(null);

    const commit = next => {
        const target = clamp(next, min, max);
        if (disabled || !Number.isFinite(target) || target === value)
            return;
        onCommit(target);
    };

    // Nothing typed, or nothing a number, keeps the value as it was.
    const commitDraft = () => {
        if (draft === null)
            return;
        const typed = parse(draft);
        setDraft(null);
        if (typed !== null)
            commit(typed);
    };

    const stepBy = (direction, event) => {
        const typed = draft === null ? null : parse(draft);
        setDraft(null);
        commit((typed ?? value) + direction * step * (event.shiftKey ? 10 : 1));
    };

    const onKeyDown = event => {
        if (disabled)
            return;
        if (event.key === 'Escape') {
            if (draft !== null) {
                event.preventDefault();
                event.stopPropagation();
                setDraft(null);
            }
            return;
        }
        const action = {
            Enter: commitDraft,
            ArrowUp: () => stepBy(1, event),
            ArrowDown: () => stepBy(-1, event),
        }[event.key];
        
        if (!action)
            return;
        event.preventDefault();
        action();
    };

    const shown = draft ?? format(value);

    return (
        <Field aria-disabled={disabled || undefined}>
            <Input
                type="text"
                inputMode="decimal"
                role="spinbutton"
                aria-label={label}
                aria-valuemin={min}
                aria-valuemax={max}
                aria-valuenow={value}
                aria-valuetext={unit ? `${format(value)} ${unit}` : format(value)}
                title={`${format(min)}–${format(max)}${unit ? ` ${unit}` : ''}: type a value and press Enter, or use the arrow keys`}
                changed={changed}
                disabled={disabled}
                value={shown}
                onChange={event => setDraft(event.target.value.replace(/[^0-9.-]/g, ''))}
                onFocus={event => event.target.select()}
                onBlur={commitDraft}
                onKeyDown={onKeyDown}
            />
            {unit && <Unit>{unit}</Unit>}
        </Field>
    );
};
export default NumberField;

const Field = Styled.div`
    display: flex;
    align-items: center;
    justify-content: flex-end;
    flex-shrink: 0;
    gap: 3px;
    box-sizing: border-box;
    width: 76px;
    height: 24px;
    padding: 0 8px;
    border: 1px solid var(--border-subtle);
    border-radius: 5px;
    background: var(--bg-inset);
    &:focus-within { outline: 2px solid var(--accent-primary); outline-offset: 1px; }
    &[aria-disabled='true'] { opacity: .5; }
`;

const Input = Styled('input', { shouldForwardProp: prop => prop !== 'changed' })`
    flex: 1;
    min-width: 0;
    padding: 0;
    border: none;
    outline: none;
    background: none;
    text-align: right;
    font: 500 11px/1 var(--font-mono);
    font-variant-numeric: tabular-nums;
    color: ${({ changed }) => changed ? 'var(--accent-primary-text)' : 'var(--text-primary)'};
    &:disabled { cursor: default; }
`;

const Unit = Styled.span`
    flex-shrink: 0;
    font: 400 10px/1 var(--font-mono);
    color: var(--text-muted);
`;
