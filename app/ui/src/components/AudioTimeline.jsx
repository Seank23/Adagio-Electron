import { useEffect, useRef, useSyncExternalStore } from 'react';
import Styled from '@emotion/styled';
import { useSelector, useStore } from 'react-redux';
import WaveSurfer from 'wavesurfer.js';
import { selectIsPlaying } from '../store/playbackSlice';
import { subscribeWaveform, getWaveform } from '../engine-client/FrameStore';
import { useEngineCommands } from '../hooks/useEngineCommands';
import { useReport } from '../hooks/useReport';
import { usePalette } from '../hooks/usePalette';
import { useStoreListener } from '../hooks/useStoreListener';
import { withAlpha } from '../utils/canvas';
import { clamp } from '../utils/math';
import { coarsestPeaks } from '../utils/waveform';

const MAX_PX_PER_SEC = 500;
const BAR_WIDTH_PX = 2;
const BAR_GAP_PX = 1;
const BAR_SPACING_PX = BAR_WIDTH_PX + BAR_GAP_PX;
// Below this a press is still a click, and seeks.
const CLICK_SLOP_PX = 4;

const selectCurrentTime = state => state.playback.currentTime;

// wavesurfer draws one bar per peak, so a set with fewer peaks than the zoom has bar
// slots leaves some slots empty. Use the coarsest set with at least one peak per bar,
// and past the finest set, repeat each peak until there is.
const createPeakPicker = (waveform, duration) => {
    const sets = waveform
        .map(set => ({ ...set, perSecond: set.peaks.length / duration }))
        .sort((a, b) => a.perSecond - b.perSecond);
    const repeated = new Map();

    return pxPerSec => {
        const barsPerSecond = pxPerSec / BAR_SPACING_PX;
        const set = sets.find(candidate => candidate.perSecond >= barsPerSecond) ?? sets[sets.length - 1];
        const repeat = Math.max(1, Math.ceil(barsPerSecond / set.perSecond));
        const key = `${set.resolution}x${repeat}`;
        if (repeat === 1)
            return { key, peaks: set.peaks };
        if (!repeated.has(key)) {
            const peaks = new Float32Array(set.peaks.length * repeat);
            for (let i = 0; i < peaks.length; i++)
                peaks[i] = set.peaks[Math.floor(i / repeat)];
            repeated.set(key, peaks);
        }
        return { key, peaks: repeated.get(key) };
    };
};

const toAudioData = (peaks, duration) => ({
    duration,
    length: peaks.length,
    sampleRate: peaks.length / duration,
    numberOfChannels: 1,
    getChannelData: () => peaks,
});

const waveColours = palette => ({
    waveColor: withAlpha(palette.text.secondary, 0.55),
    progressColor: palette.accent.primaryFg,
    cursorColor: palette.text.primary,
});

// A click seeks, zoom and pan are driven from outside, through apiRef: { zoomBy, fit, centerOn,
// zoomAround, view, setInteracting }. onView hears the visible range after every zoom and scroll.
const AudioTimeline = ({ apiRef, onView }) => {
    const store = useStore();
    const palette = usePalette();
    const commands = useEngineCommands();
    const report = useReport();
    const waveform = useSyncExternalStore(subscribeWaveform, getWaveform);
    const duration = useSelector(state => state.playback.duration);
    const follow = useSelector(state => state.settings.follow);
    const isPlaying = useSelector(selectIsPlaying);

    const hostRef = useRef(null);
    const timelineRef = useRef(null);
    const latestRef = useRef(null);

    useEffect(() => {
        latestRef.current = { palette, follow, isPlaying, onView, commands, report };
    });

    useEffect(() => {
        const host = hostRef.current;
        if (!host || !waveform || duration <= 0)
            return;

        const coarsest = coarsestPeaks(waveform);
        const trackPeak = coarsest.peaks.reduce((max, peak) => Math.max(max, Math.abs(peak)), 0) || 1;
        const pickPeaks = createPeakPicker(waveform, duration);
        const view = { px: 0, peaksKey: `${coarsest.resolution}x1`, fitted: true, height: host.clientHeight, interacting: false, press: null, ready: false };

        const ws = WaveSurfer.create({
            container: host,
            height: Math.max(8, view.height),
            ...waveColours(latestRef.current.palette),
            cursorWidth: 1,
            barWidth: BAR_WIDTH_PX,
            barGap: BAR_GAP_PX,
            normalize: true,
            maxPeak: trackPeak,
            interact: false,
            hideScrollbar: true,
            autoScroll: false,
            autoCenter: false,
        });
        ws.setMuted(true);

        const width = () => ws.getWidth() || host.clientWidth;
        const fitPx = () => width() / duration;
        const localX = event => event.clientX - host.getBoundingClientRect().left;
        const timeAt = x => (ws.getScroll() + x) / view.px;

        const emitView = () => {
            if (!view.ready)
                return;
            const start = ws.getScroll() / view.px;
            latestRef.current.onView?.({ start, end: Math.min(duration, start + width() / view.px), pxPerSec: view.px });
        };

        const setZoom = (px, anchorTime, anchorX) => {
            const fit = fitPx();
            view.px = clamp(px, fit, Math.max(MAX_PX_PER_SEC, fit));
            view.fitted = view.px <= fit;
            ws.zoom(view.px);
            const { key, peaks } = pickPeaks(view.px);
            if (key !== view.peaksKey) {
                view.peaksKey = key;
                ws.getRenderer().render(toAudioData(peaks, duration));
            }
            if (anchorTime !== undefined)
                ws.setScroll(anchorTime * view.px - anchorX);
            emitView();
        };

        const centerOn = time => {
            ws.setScroll(time * view.px - width() / 2);
            emitView();
        };

        const api = {
            zoomBy: factor => {
                const middle = width() / 2;
                setZoom(view.px * factor, timeAt(middle), middle);
            },
            fit: () => {
                setZoom(fitPx());
                ws.setScroll(0);
                emitView();
            },
            centerOn,
            zoomAround: setZoom,
            view: () => ({ px: view.px, start: ws.getScroll() / view.px, width: width(), minPx: fitPx(), maxPx: Math.max(MAX_PX_PER_SEC, fitPx()) }),
            setInteracting: interacting => {
                view.interacting = interacting;
            },
        };

        // The playhead, and the view following it. While playing, Follow keeps it centred;
        // while paused, it only brings it back into view, so clicking to seek doesn't
        // move the view under the pointer.
        const showTime = time => {
            if (!view.ready)
                return;
            ws.seekTo(clamp(time / duration, 0, 1));
            const latest = latestRef.current;
            if (!latest.follow || view.interacting)
                return;
            const x = time * view.px - ws.getScroll();
            if (latest.isPlaying ? Math.abs(x - width() / 2) > 1 : x < 0 || x > width())
                centerOn(time);
        };
        timelineRef.current = { showTime, recolour: palette => ws.setOptions({ ...waveColours(palette), minPxPerSec: view.px }) };

        const onPointerDown = event => {
            if (event.button === 0 && view.ready)
                view.press = { id: event.pointerId, x: event.clientX, y: event.clientY };
        };
        const onPointerUp = event => {
            const press = view.press;
            view.press = null;
            if (!press || press.id !== event.pointerId)
                return;
            if (Math.hypot(event.clientX - press.x, event.clientY - press.y) >= CLICK_SLOP_PX)
                return;
            const { commands, report } = latestRef.current;
            commands.seek(clamp(timeAt(localX(event)), 0, duration)).then(report);
        };
        const onPointerCancel = () => {
            view.press = null;
        };
        host.addEventListener('pointerdown', onPointerDown);
        host.addEventListener('pointerup', onPointerUp);
        host.addEventListener('pointercancel', onPointerCancel);

        const resizeObserver = new ResizeObserver(() => {
            if (!view.ready)
                return;
            const height = Math.max(8, host.clientHeight);
            if (height !== view.height) {
                view.height = height;
                ws.setOptions({ height, minPxPerSec: view.px });
            }
            if (view.fitted || view.px < fitPx())
                setZoom(fitPx());
            else
                emitView();
        });
        resizeObserver.observe(host);

        ws.on('scroll', emitView);

        let destroyed = false;
        ws.load(null, [Float32Array.from(coarsest.peaks)], duration)
            .then(() => {
                if (destroyed)
                    return;
                view.ready = true;
                setZoom(fitPx());
                apiRef.current = api;
                showTime(store.getState().playback.currentTime);
            })
            .catch(() => undefined);

        return () => {
            destroyed = true;
            resizeObserver.disconnect();
            host.removeEventListener('pointerdown', onPointerDown);
            host.removeEventListener('pointerup', onPointerUp);
            host.removeEventListener('pointercancel', onPointerCancel);
            timelineRef.current = null;
            apiRef.current = null;
            ws.destroy();
        };
    }, [waveform, duration, store, apiRef]);

    // A theme change recolours the waveform in place rather than rebuilding it.
    useEffect(() => {
        timelineRef.current?.recolour(palette);
    }, [palette]);

    useEffect(() => {
        if (follow)
            timelineRef.current?.showTime(store.getState().playback.currentTime);
    }, [follow, store]);

    useStoreListener(selectCurrentTime, time => timelineRef.current?.showTime(time));

    return <Host ref={hostRef} role="group" aria-label="Waveform" />;
};
export default AudioTimeline;

const Host = Styled.div`
    flex: 1;
    min-height: 0;
    overflow: hidden;
    user-select: none;
    background: var(--bg-inset);
`;
