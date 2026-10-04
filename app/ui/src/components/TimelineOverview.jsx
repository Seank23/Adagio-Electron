import { useEffect, useRef, useSyncExternalStore } from 'react';
import Styled from '@emotion/styled';
import { useSelector, useStore } from 'react-redux';
import { subscribeWaveform, getWaveform } from '../engine-client/FrameStore';
import { usePalette } from '../hooks/usePalette';
import { useStoreListener } from '../hooks/useStoreListener';
import { fitToElement, withAlpha } from '../utils/canvas';
import { clamp } from '../utils/math';
import { coarsestPeaks, peakColumns } from '../utils/waveform';

const ZOOM_PER_PX = 1 / 200;
const PAN_SENSITIVITY = 0.5;
const DRAG_THRESHOLD_PX = 4;

const selectCurrentTime = state => state.playback.currentTime;

// The whole track from its coarsest peaks, the zoomed view's window and the playhead,
// for panning and zooming
const TimelineOverview = ({ viewRef, apiRef, drawRef }) => {
    const store = useStore();
    const palette = usePalette();
    const waveform = useSyncExternalStore(subscribeWaveform, getWaveform);
    const duration = useSelector(state => state.playback.duration);
    const canvasRef = useRef(null);

    useEffect(() => {
        const canvas = canvasRef.current;
        if (!canvas)
            return;
        const context = canvas.getContext('2d');
        const peaks = waveform ? coarsestPeaks(waveform).peaks : null;
        let columns = null;

        const draw = () => {
            const ratio = fitToElement(canvas);
            const width = canvas.clientWidth;
            const height = canvas.clientHeight;
            context.setTransform(ratio, 0, 0, ratio, 0, 0);
            context.clearRect(0, 0, width, height);
            if (!peaks || duration <= 0)
                return;

            const count = Math.floor(width);
            if (columns?.length !== count)
                columns = peakColumns(peaks, count);

            const playX = store.getState().playback.currentTime / duration * width;
            const middle = height / 2;
            const amplitude = middle - 2;
            const bars = (from, to, colour) => {
                context.fillStyle = colour;
                for (let x = from; x < to; x++) {
                    const half = Math.max(0.5, columns[x] * amplitude);
                    context.fillRect(x, middle - half, 1, half * 2);
                }
            };
            const split = Math.min(count, Math.max(0, Math.round(playX)));
            bars(0, split, withAlpha(palette.accent.primaryFg, 0.55));
            bars(split, count, palette.text.muted);

            const view = viewRef.current;
            if (view) {
                const x0 = view.start / duration * width;
                const x1 = view.end / duration * width;
                context.beginPath();
                context.roundRect(x0 + 0.5, 0.5, Math.max(2, x1 - x0) - 1, height - 1, 3);
                context.fillStyle = withAlpha(palette.text.primary, 0.06);
                context.fill();
                context.lineWidth = 1;
                context.strokeStyle = palette.text.secondary;
                context.stroke();
            }

            context.fillStyle = palette.text.primary;
            context.fillRect(Math.round(playX), 0, 1, height);
        };

        drawRef.current = draw;
        draw();
        const resizeObserver = new ResizeObserver(draw);
        resizeObserver.observe(canvas);

        const panSecondsPerPx = () => duration / canvas.getBoundingClientRect().width * PAN_SENSITIVITY;

        const timeAt = event => {
            const rect = canvas.getBoundingClientRect();
            return Math.min(Math.max((event.clientX - rect.left) / rect.width, 0), 1) * duration;
        };

        // Like Ableton's overview: press to grab a time, then drag vertically to zoom
        // around it and horizontally to move the view with the pointer. A press outside
        // the window first centres the view on the pressed time.
        let drag = null;
        let frame = 0;
        const isLocked = () => document.pointerLockElement === canvas;

        const applyDrag = () => {
            frame = 0;
            if (!drag?.moved || !apiRef.current)
                return;
            apiRef.current.zoomAround(drag.px * 2 ** (drag.dy * ZOOM_PER_PX), drag.time + drag.dx * panSecondsPerPx(), drag.anchorX);
        };

        const lockPointer = () => {
            try {
                canvas.requestPointerLock()?.catch?.(onLockError);
            } catch {
                onLockError();
            }
        };

        const endDrag = () => {
            if (!drag)
                return;
            if (frame) {
                cancelAnimationFrame(frame);
                applyDrag();
            }
            drag = null;
            canvas.style.cursor = '';
            if (isLocked())
                document.exitPointerLock();
            apiRef.current?.setInteracting(false);
        };

        const onPointerDown = event => {
            const api = apiRef.current;
            if (event.button !== 0 || !api)
                return;
            canvas.setPointerCapture(event.pointerId);
            const time = timeAt(event);
            const { px, start, width, minPx, maxPx } = api.view();
            let anchorX = (time - start) * px;
            if (anchorX < 0 || anchorX > width) {
                api.centerOn(time);
                anchorX = width / 2;
            }
            const secondsPerPx = panSecondsPerPx();
            drag = {
                id: event.pointerId, x0: event.clientX, y0: event.clientY, lastX: event.clientX, lastY: event.clientY,
                time, anchorX, px, dx: 0, dy: 0, moved: false, locking: false, wasLocked: false,
                dxRange: [-time / secondsPerPx, (duration - time) / secondsPerPx],
                dyRange: [Math.log2(minPx / px) / ZOOM_PER_PX, Math.log2(maxPx / px) / ZOOM_PER_PX],
            };
            api.setInteracting(true);
        };

        const onPointerMove = event => {
            if (!drag || event.pointerId !== drag.id)
                return;
            if (!drag.moved) {
                if (Math.hypot(event.clientX - drag.x0, event.clientY - drag.y0) < DRAG_THRESHOLD_PX)
                    return;
                drag.moved = true;
                canvas.style.cursor = 'grabbing';
                drag.dx = event.clientX - drag.x0;
                drag.dy = event.clientY - drag.y0;
                drag.locking = true;
                lockPointer();
            } else if (isLocked()) {
                // Locked, clientX and clientY stop changing; only the movement is reported.
                drag.dx += event.movementX;
                drag.dy += event.movementY;
            } else {
                drag.dx += event.clientX - drag.lastX;
                drag.dy += event.clientY - drag.lastY;
            }
            drag.lastX = event.clientX;
            drag.lastY = event.clientY;
            drag.dx = clamp(drag.dx, ...drag.dxRange);
            drag.dy = clamp(drag.dy, ...drag.dyRange);
            if (!frame)
                frame = requestAnimationFrame(applyDrag);
        };

        const onPointerUp = event => {
            if (drag && event.pointerId === drag.id)
                endDrag();
        };

        const onLostCapture = event => {
            if (drag && event.pointerId === drag.id && !drag.locking)
                endDrag();
        };

        const onLockChange = () => {
            if (!drag)
                return;
            if (isLocked())
                drag.wasLocked = true;
            else if (drag.wasLocked)
                endDrag();
        };

        const onLockError = () => {
            if (drag)
                drag.locking = false;
        };

        canvas.addEventListener('pointerdown', onPointerDown);
        canvas.addEventListener('lostpointercapture', onLostCapture);
        window.addEventListener('pointermove', onPointerMove);
        window.addEventListener('pointerup', onPointerUp);
        window.addEventListener('pointercancel', onPointerUp);
        document.addEventListener('pointerlockchange', onLockChange);
        document.addEventListener('pointerlockerror', onLockError);

        return () => {
            endDrag();
            resizeObserver.disconnect();
            canvas.removeEventListener('pointerdown', onPointerDown);
            canvas.removeEventListener('lostpointercapture', onLostCapture);
            window.removeEventListener('pointermove', onPointerMove);
            window.removeEventListener('pointerup', onPointerUp);
            window.removeEventListener('pointercancel', onPointerUp);
            document.removeEventListener('pointerlockchange', onLockChange);
            document.removeEventListener('pointerlockerror', onLockError);
            drawRef.current = null;
        };
    }, [waveform, duration, palette, store, viewRef, apiRef, drawRef]);

    useStoreListener(selectCurrentTime, () => drawRef.current?.());

    return <Canvas ref={canvasRef} role="img" aria-label="Track overview" />;
};
export default TimelineOverview;

const Canvas = Styled.canvas`
    display: block;
    flex-shrink: 0;
    width: 100%;
    height: 22px;
    border-radius: 4px;
    background: var(--bg-inset);
    cursor: grab;
    touch-action: none;
`;
