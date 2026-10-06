import { useRef, useState } from 'react';
import Styled from '@emotion/styled';

// − value + over an integer range.
const NumberStepper = ({ label, value, min, max, onChange, disabled = false }) => {
    const pendingRef = useRef(null);
    // The text being typed, or null when not editing.
    const [draft, setDraft] = useState(null);

    const commit = async next => {
        const target = Math.min(max, Math.max(min, next));
        if (disabled || target === (pendingRef.current ?? value))
            return;
        pendingRef.current = target;
        try {
            await onChange(target);
        } finally {
            if (pendingRef.current === target)
                pendingRef.current = null;
        }
    };

    const current = () => {
        const typed = draft === null ? NaN : parseInt(draft, 10);
        return Number.isFinite(typed) ? typed : pendingRef.current ?? value;
    };

    const step = direction => {
        const from = current();
        setDraft(null);
        commit(from + direction);
    };

    // Nothing typed, or nothing a number, keeps the value as it was.
    const commitDraft = () => {
        if (draft === null)
            return;
        const typed = parseInt(draft, 10);
        setDraft(null);
        if (Number.isFinite(typed))
            commit(typed);
    };

    const onKeyDown = event => {
        if (disabled)
            return;
        if (event.key === 'Escape') {
            // Cancels the edit; with nothing typed, Esc is left to close whatever holds this.
            if (draft !== null) {
                event.preventDefault();
                event.stopPropagation();
                setDraft(null);
            }
            return;
        }
        const action = {
            Enter: commitDraft,
            ArrowUp: () => step(1),
            ArrowDown: () => step(-1),
            Home: () => { setDraft(null); commit(min); },
            End: () => { setDraft(null); commit(max); },
        }[event.key];
        if (!action)
            return;
        event.preventDefault();
        action();
    };

    // Digits only, and no more of them than the maximum has.
    const onInput = event => setDraft(event.target.value.replace(/\D/g, '').slice(0, String(max).length));

    const known = value !== null && value !== undefined;
    const shown = draft ?? (known ? String(value) : '–');

    return (
        <Track aria-disabled={disabled || undefined}>
            <StepButton type="button" aria-label={`Decrease ${label}`} disabled={disabled || !known || value <= min}
                onClick={() => step(-1)}>
                −
            </StepButton>
            <Value
                type="text"
                inputMode="numeric"
                role="spinbutton"
                aria-label={label}
                aria-valuemin={min}
                aria-valuemax={max}
                aria-valuenow={known ? value : undefined}
                title={`${min}–${max}: type a value and press Enter, or use the arrow keys`}
                disabled={disabled || !known}
                value={shown}
                onChange={onInput}
                onFocus={event => event.target.select()}
                onBlur={commitDraft}
                onKeyDown={onKeyDown}
            />
            <StepButton type="button" aria-label={`Increase ${label}`} disabled={disabled || !known || value >= max}
                onClick={() => step(1)}>
                +
            </StepButton>
        </Track>
    );
};
export default NumberStepper;

const Track = Styled.div`
    display: inline-flex;
    align-items: center;
    gap: 2px;
    padding: 2px;
    border-radius: 6px;
    background: var(--bg-surface);
    &[aria-disabled='true'] { opacity: .5; }
`;

const StepButton = Styled.button`
    display: flex;
    align-items: center;
    justify-content: center;
    width: 24px;
    height: 22px;
    padding: 0;
    border: none;
    border-radius: 5px;
    background: none;
    color: var(--text-secondary);
    font: 500 13px/1 var(--font-ui);
    cursor: pointer;
    &:hover:not(:disabled) { background: var(--bg-raised); color: var(--text-primary); }
    &:disabled { cursor: default; opacity: .5; }
    &:focus-visible { outline: 2px solid var(--accent-primary); outline-offset: 1px; }
`;

const Value = Styled.input`
    box-sizing: border-box;
    width: 32px;
    height: 22px;
    padding: 0 4px;
    border: none;
    border-radius: 5px;
    background: var(--bg-raised);
    text-align: center;
    font: 500 11px/1.2 var(--font-mono);
    font-variant-numeric: tabular-nums;
    color: var(--accent-primary-text);
    &:focus { outline: 2px solid var(--accent-primary); outline-offset: 1px; }
    &:disabled { cursor: default; }
`;
