import { useEffect, useRef, useState } from 'react';
import Styled from '@emotion/styled';
import { useDispatch, useSelector, useStore } from 'react-redux';
import Caption from './controls/Caption';
import { useEngineCommands } from '../hooks/useEngineCommands';
import { useReport } from '../hooks/useReport';
import { selectIsPaused } from '../store/playbackSlice';
import {
    TUNING_KEY, TUNING_STAGE, selectHasSchema, selectTuningHz, selectTuningMax, selectTuningMin, setSettingValue,
} from '../store/pipelineSlice';
import { A4_DEFAULT_HZ, centsFrom440, formatCents } from '../utils/music';
import { clamp } from '../utils/math';

const STEP_HZ = 0.5;
const FINE_STEP_HZ = 0.1;
const DRAG_HZ_PER_PX = 0.1;
const FINE_DRAG_HZ_PER_PX = 0.02;
const DRAG_THRESHOLD_PX = 3;
const SEND_INTERVAL_MS = 50;

const roundTenth = hz => Math.round(hz * 10) / 10;

const TuningControl = () => {
    const dispatch = useDispatch();
    const store = useStore();
    const commands = useEngineCommands();
    const report = useReport();

    const tuningHz = useSelector(selectTuningHz);
    const min = useSelector(selectTuningMin);
    const max = useSelector(selectTuningMax);
    const hasSchema = useSelector(selectHasSchema);
    const connected = useSelector(state => state.app.connectionState === 'connected');
    const disabled = !connected || !hasSchema;

    // Quick clicks step from the value in flight, not the one the engine last confirmed.
    const pendingRef = useRef(null);
    const dragRef = useRef(null);
    const [dragHz, setDragHz] = useState(null);

    const commit = async hz => {
        const value = clamp(roundTenth(hz), min, max);
        if (!Number.isFinite(value) || value === (pendingRef.current ?? tuningHz))
            return;
        pendingRef.current = value;
        const result = await commands.setAnalysisSetting(TUNING_STAGE, TUNING_KEY, value);
        if (pendingRef.current === value)
            pendingRef.current = null;
        if (!result.ok) {
            report(result);
            return;
        }
        dispatch(setSettingValue({ stage: TUNING_STAGE, key: TUNING_KEY, value }));
        if (selectIsPaused(store.getState()))
            commands.analyseFrame().then(report);
    };

    const step = (direction, event) => {
        const size = event.shiftKey ? FINE_STEP_HZ : STEP_HZ;
        commit((pendingRef.current ?? tuningHz) + direction * size);
    };

    // Dragging the value: up raises it, down lowers it. Once it moves, the pointer is
    // locked so the drag never runs out of screen, as on the timeline overview.
    const sendThrottled = drag => {
        const elapsed = performance.now() - drag.lastSent;
        if (elapsed >= SEND_INTERVAL_MS) {
            clearTimeout(drag.timer);
            drag.timer = 0;
            drag.lastSent = performance.now();
            commit(drag.value);
        } else if (!drag.timer) {
            drag.timer = setTimeout(() => {
                drag.timer = 0;
                drag.lastSent = performance.now();
                commit(drag.value);
            }, SEND_INTERVAL_MS - elapsed);
        }
    };

    const endDrag = () => {
        const drag = dragRef.current;
        if (!drag)
            return;
        dragRef.current = null;
        clearTimeout(drag.timer);
        if (document.pointerLockElement === drag.element)
            document.exitPointerLock();
        if (drag.moved)
            commit(drag.value);
        setDragHz(null);
    };

    const onPointerDown = event => {
        if (event.button !== 0 || disabled)
            return;
        event.currentTarget.setPointerCapture(event.pointerId);
        dragRef.current = {
            id: event.pointerId, element: event.currentTarget, y0: event.clientY, lastY: event.clientY,
            value: tuningHz, moved: false, locking: false, lastSent: 0, timer: 0,
        };
    };

    const onPointerMove = event => {
        const drag = dragRef.current;
        if (!drag || event.pointerId !== drag.id)
            return;
        let dy;
        if (!drag.moved) {
            if (Math.abs(event.clientY - drag.y0) < DRAG_THRESHOLD_PX)
                return;
            drag.moved = true;
            drag.locking = true;
            dy = event.clientY - drag.y0;
            try {
                drag.element.requestPointerLock()?.catch?.(() => {
                    drag.locking = false;
                });
            } catch {
                drag.locking = false;
            }
        } else {
            // Locked, clientY stops changing; only the movement is reported.
            dy = document.pointerLockElement === drag.element ? event.movementY : event.clientY - drag.lastY;
        }
        drag.lastY = event.clientY;
        // Up is a negative dy, and raises the reference.
        const rate = event.shiftKey ? FINE_DRAG_HZ_PER_PX : DRAG_HZ_PER_PX;
        drag.value = clamp(drag.value - dy * rate, min, max);
        setDragHz(roundTenth(drag.value));
        sendThrottled(drag);
    };

    const onLostPointerCapture = event => {
        const drag = dragRef.current;
        if (drag && event.pointerId === drag.id && !drag.locking)
            endDrag();
    };

    // The lock ending by itself (Escape, switching windows) ends the drag too; the release
    // that would have ended it may now land elsewhere.
    const endDragRef = useRef(endDrag);
    useEffect(() => {
        endDragRef.current = endDrag;
    });
    useEffect(() => {
        const onLockChange = () => {
            const drag = dragRef.current;
            if (!drag)
                return;
            if (document.pointerLockElement === drag.element)
                drag.wasLocked = true;
            else if (drag.wasLocked)
                endDragRef.current();
        };
        document.addEventListener('pointerlockchange', onLockChange);
        return () => document.removeEventListener('pointerlockchange', onLockChange);
    }, []);

    const onKeyDown = event => {
        const direction = event.key === 'ArrowUp' ? 1 : event.key === 'ArrowDown' ? -1 : 0;
        if (!direction || disabled)
            return;
        event.preventDefault();
        step(direction, event);
    };

    const shownHz = dragHz ?? tuningHz;

    return (
        <Control>
            <Caption>Tuning</Caption>
            <Stepper aria-disabled={disabled}>
                <StepButton type="button" aria-label="Lower the A4 reference" title="Lower (Shift: 0.1 Hz)"
                    disabled={disabled || tuningHz <= min} onClick={event => step(-1, event)}>
                    −
                </StepButton>
                <Value
                    role="slider"
                    tabIndex={disabled ? -1 : 0}
                    aria-label="A440 reference"
                    aria-valuemin={min}
                    aria-valuemax={max}
                    aria-valuenow={shownHz}
                    aria-valuetext={`${shownHz.toFixed(1)} Hz`}
                    aria-disabled={disabled}
                    title="Drag up or down to tune (Shift: finer) · double-click for 440 Hz"
                    dragging={dragHz !== null}
                    onPointerDown={onPointerDown}
                    onPointerMove={onPointerMove}
                    onPointerUp={endDrag}
                    onPointerCancel={endDrag}
                    onLostPointerCapture={onLostPointerCapture}
                    onDoubleClick={() => !disabled && commit(A4_DEFAULT_HZ)}
                    onKeyDown={onKeyDown}
                >
                    <ValueLabel>A4</ValueLabel>
                    <ValueHz>{shownHz.toFixed(1)} Hz</ValueHz>
                </Value>
                <StepButton type="button" aria-label="Raise the A4 reference" title="Raise (Shift: 0.1 Hz)"
                    disabled={disabled || tuningHz >= max} onClick={event => step(1, event)}>
                    +
                </StepButton>
            </Stepper>
            <Offset title="Offset from A440">{formatCents(centsFrom440(shownHz), 1)}</Offset>
        </Control>
    );
};
export default TuningControl;

const Control = Styled.div`
    display: flex;
    align-items: center;
    flex-shrink: 0;
    gap: 8px;
`;

const Stepper = Styled.div`
    display: flex;
    align-items: center;
    height: 24px;
    padding: 0 2px;
    border: 1px solid var(--border-subtle);
    border-radius: 6px;
    background: var(--bg-inset);
    &[aria-disabled='true'] { opacity: .5; }
`;

const StepButton = Styled.button`
    display: flex;
    align-items: center;
    justify-content: center;
    width: 20px;
    height: 20px;
    padding: 0;
    border: none;
    border-radius: 4px;
    background: none;
    color: var(--text-secondary);
    font: 500 13px/1 var(--font-ui);
    cursor: pointer;
    &:hover:not(:disabled) { background: var(--bg-surface); color: var(--text-primary); }
    &:disabled { cursor: default; opacity: .5; }
    &:focus-visible { outline: 2px solid var(--accent-primary); outline-offset: 1px; }
`;

const Value = Styled('div', { shouldForwardProp: prop => prop !== 'dragging' })`
    display: flex;
    align-items: baseline;
    gap: 5px;
    padding: 4px 6px;
    border-radius: 4px;
    cursor: ns-resize;
    user-select: none;
    touch-action: none;
    background: ${({ dragging }) => dragging ? 'var(--bg-surface)' : 'none'};
    &:hover:not([aria-disabled='true']) { background: var(--bg-surface); }
    &[aria-disabled='true'] { cursor: default; }
    &:focus-visible { outline: 2px solid var(--accent-primary); outline-offset: 1px; }
`;

const ValueLabel = Styled.span`
    font: 400 10px/1 var(--font-mono);
    color: var(--text-muted);
`;

const ValueHz = Styled.span`
    font: 500 11px/1 var(--font-mono);
    font-variant-numeric: tabular-nums;
    color: var(--text-primary);
`;

const Offset = Styled.span`
    min-width: 38px;
    font: 400 10px/1 var(--font-mono);
    font-variant-numeric: tabular-nums;
    color: var(--text-muted);
`;
