import { useEffect, useRef, useSyncExternalStore } from 'react';
import { useSelector, useDispatch } from 'react-redux';
import { setCurrentTime } from '../store/playbackSlice';
import { useEngineCommands } from '../hooks/useEngineCommands';
import { subscribeWaveform, getWaveform } from '../engine-client/FrameStore';
import { usePalette } from '../hooks/usePalette';
import WaveSurfer from 'wavesurfer.js';
import TimelinePlugin from "wavesurfer.js/dist/plugins/timeline";

const AudioTimeline = () => {
    const palette = usePalette();
    const dispatch = useDispatch();
    const commands = useEngineCommands();
    const waveformData = useSyncExternalStore(subscribeWaveform, getWaveform);
    const duration = useSelector(state => state.playback.duration);
    const currentTime = useSelector(state => state.playback.currentTime);

    const containerRef = useRef(null);
    const timelineRef = useRef(null);
    const waveSurferRef = useRef(null);

    const minPxPerSec = useRef(1);
    const currentWaveformRef = useRef(null);
    const waveformBusyRef = useRef(false);

    const handleSeek = time => {
        dispatch(setCurrentTime(time));
        commands.seek(time);
    }

    const getWaveformResolution = minPxPerSec => {
        if (minPxPerSec > 200) return waveformData?.find(data => data.resolution === 512)?.peaks;
        if (minPxPerSec > 100) return waveformData?.find(data => data.resolution === 1024)?.peaks;
        if (minPxPerSec > 50) return waveformData?.find(data => data.resolution === 2048)?.peaks;
        if (minPxPerSec > 25) return waveformData?.find(data => data.resolution === 4096)?.peaks;
        return waveformData?.find(data => data.resolution === 8192)?.peaks;
    };

    useEffect(() => {
        if (!containerRef.current || duration === 0 || !waveformData) return;

        const waveSurfer = WaveSurfer.create({
            container: containerRef.current,
            height: 120,
            ...waveColours(palette),
            barWidth: 2,
            barGap: 0,
            interact: true,
            normalize: true,
            minPxPerSec: minPxPerSec.current,
            plugins: [
                TimelinePlugin.create({
                    container: timelineRef.current
                })
            ]
        });
        waveSurfer.setMuted(true);
        currentWaveformRef.current = getWaveformResolution(minPxPerSec.current);
        waveSurfer.load(null, currentWaveformRef.current, duration);
        waveSurfer.on('interaction', time => handleSeek(time));
        waveSurferRef.current = waveSurfer;

        return () => {
            waveSurfer.destroy();
        };
    }, [waveformData, duration]);

    // A theme change recolours the waveform in place rather than rebuilding it.
    useEffect(() => {
        waveSurferRef.current?.setOptions(waveColours(palette));
    }, [palette]);

    useEffect(() => {
        if (!waveSurferRef.current || !duration || waveformBusyRef.current) return;
        const ratio = currentTime / duration;
        waveSurferRef.current.seekTo(Math.min(Math.max(ratio, 0), 1));
    }, [currentTime]);

    return (
        <div style={viewportStyle}>
            <div style={containerStyle} ref={containerRef} />
            <div style={{ width: '100%' }} ref={timelineRef} />
        </div>
    )
};
export default AudioTimeline;

const waveColours = palette => ({
    waveColor: palette.text.secondary,
    progressColor: palette.accent.primaryFg,
    cursorColor: palette.text.primary,
});

const viewportStyle = {
    width: '100%',
    overflow: 'hidden',
    position: 'relative'
};
const containerStyle = {
    width: '100%',
    '& > div': {
        transformOrigin: 'center center',
        willChange: 'transform',
    },
};