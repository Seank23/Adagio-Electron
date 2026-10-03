import { useEffect, useRef } from 'react';
import Styled from '@emotion/styled';
import { useStore } from 'react-redux';
import { usePalette } from '../hooks/usePalette';
import { useStoreListener } from '../hooks/useStoreListener';
import { fitToElement } from '../utils/canvas';
import { formatDuration } from '../utils/format';
import { FONTS } from '../theme/tokens';

const HEIGHT = 20;
const MIN_LABEL_GAP = 56;
const MIN_TICK_GAP = 4;
// [seconds between labels, ticks per label]
const STEPS = [
    [0.1, 10], [0.2, 4], [0.5, 5], [1, 10], [2, 4], [5, 5], [10, 10], [15, 3],
    [30, 6], [60, 6], [120, 4], [300, 5], [600, 10],
];

const selectCurrentTime = state => state.playback.currentTime;

// 70 → '1:10'; 70.5 → '1:10.5', for the sub-second labels when zoomed in.
const formatRulerTime = seconds => {
    const tenths = Math.round(seconds * 10) % 10;
    return tenths === 0 ? formatDuration(seconds) : `${formatDuration(Math.floor(seconds))}.${tenths}`;
};

const TimelineRuler = ({ viewRef, drawRef }) => {
    const store = useStore();
    const palette = usePalette();
    const canvasRef = useRef(null);

    useEffect(() => {
        const canvas = canvasRef.current;
        if (!canvas)
            return;
        const context = canvas.getContext('2d');

        const draw = () => {
            const ratio = fitToElement(canvas);
            const width = canvas.clientWidth;
            context.setTransform(ratio, 0, 0, ratio, 0, 0);
            context.clearRect(0, 0, width, HEIGHT);
            const view = viewRef.current;
            if (!view || view.pxPerSec <= 0)
                return;

            const { start, pxPerSec } = view;
            const [labelStep, ticksPerLabel] = STEPS.find(([step]) => step * pxPerSec >= MIN_LABEL_GAP) ?? STEPS[STEPS.length - 1];
            const showTicks = (labelStep / ticksPerLabel) * pxPerSec >= MIN_TICK_GAP;
            const tickStep = showTicks ? labelStep / ticksPerLabel : labelStep;
            const perLabel = showTicks ? ticksPerLabel : 1;

            context.font = `400 9px ${FONTS.mono}`;
            context.textBaseline = 'top';
            for (let i = Math.floor(start / tickStep); ; i++) {
                const x = Math.round((i * tickStep - start) * pxPerSec);
                if (x > width)
                    break;
                const position = ((i % perLabel) + perLabel) % perLabel;
                const labelled = position === 0;
                const middle = perLabel % 2 === 0 && position === perLabel / 2;
                const top = labelled ? 8 : middle ? 13 : 16;
                context.fillStyle = palette.border.strong;
                context.fillRect(x, top, 1, HEIGHT - top);
                if (labelled) {
                    context.fillStyle = palette.text.muted;
                    context.fillText(formatRulerTime(i * tickStep), x + 3, 1);
                }
            }

            // The playhead: a marker here, continued by the waveform's own cursor below.
            const playX = Math.round((store.getState().playback.currentTime - start) * pxPerSec);
            if (playX >= 0 && playX <= width) {
                context.fillStyle = palette.text.primary;
                context.fillRect(playX, 4, 1, HEIGHT - 4);
                context.beginPath();
                context.moveTo(playX - 4.5, 4);
                context.lineTo(playX + 5.5, 4);
                context.lineTo(playX + 0.5, 11);
                context.closePath();
                context.fill();
            }
        };

        drawRef.current = draw;
        draw();
        const resizeObserver = new ResizeObserver(draw);
        resizeObserver.observe(canvas);
        return () => {
            resizeObserver.disconnect();
            drawRef.current = null;
        };
    }, [palette, store, viewRef, drawRef]);

    useStoreListener(selectCurrentTime, () => drawRef.current?.());

    return <Canvas ref={canvasRef} />;
};
export default TimelineRuler;

const Canvas = Styled.canvas`
    display: block;
    flex-shrink: 0;
    width: 100%;
    height: ${HEIGHT}px;
`;
