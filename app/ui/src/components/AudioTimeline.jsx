import { useEffect, useRef, useState } from 'react';
import { useSelector, useDispatch } from 'react-redux';
import { theme } from 'antd';
import { setCurrentTime } from '../store/PlaybackSlice';
import WaveSurfer from 'wavesurfer.js';
import TimelinePlugin from "wavesurfer.js/dist/plugins/timeline";

const AudioTimeline = () => {
    const { token } = theme.useToken();
    const dispatch = useDispatch();
    const waveformData = useSelector(state => state.playback.waveformData);
    const duration = useSelector(state => state.playback.duration);
    const currentTime = useSelector(state => state.playback.currentTime);

    const containerRef = useRef(null);
    const timelineRef = useRef(null);
    const waveSurferRef = useRef(null);

    const [waveSurferInitialized, setWaveSurferInitialized] = useState(false);
    const minPxPerSec = useRef(10);
    const [currentWaveform, setCurrentWaveform] = useState(null);

    const handleSeek = time => {
        dispatch(setCurrentTime(time));
        window.api.seek(time);
    }

    const getWaveformResolution = minPxPerSec => {
        if (minPxPerSec > 200) return waveformData?.find(data => data.resolution === 512)?.peaks.map(p => p.max).flat();
        if (minPxPerSec > 100) return waveformData?.find(data => data.resolution === 1024)?.peaks.map(p => p.max).flat();
        if (minPxPerSec > 50) return waveformData?.find(data => data.resolution === 2048)?.peaks.map(p => p.max).flat();
        if (minPxPerSec > 25) return waveformData?.find(data => data.resolution === 4096)?.peaks.map(p => p.max).flat();
        return waveformData?.find(data => data.resolution === 8192)?.peaks.map(p => p.max).flat();
    };

    useEffect(() => {
        if (!containerRef.current || duration === 0 || !waveformData) return;

        const waveSurfer = WaveSurfer.create({
            container: containerRef.current,
            height: 120,
            waveColor: '#666',
            progressColor: token.colorPrimary,
            cursorColor: token.colorPrimaryActive,
            barWidth: 2,
            barGap: 0,
            responsive: true,
            interact: true,
            normalize: true,
            plugins: [
                TimelinePlugin.create({
                    container: timelineRef.current
                })
            ]
        });
        waveSurfer.setMuted(true);
        waveSurfer.load(null, getWaveformResolution(minPxPerSec), duration);
        waveSurfer.on('interaction', time => handleSeek(time));
        waveSurferRef.current = waveSurfer;
        setWaveSurferInitialized(true);

        return () => {
            waveSurfer.destroy();
        };
    }, [waveformData, duration]);

    useEffect(() => {
        if (!waveSurferRef.current) return;
        waveSurferRef.current.load(null, currentWaveform, duration);
        setTimeout(() => {
            waveSurferRef.current.zoom(minPxPerSec.current);
            waveSurferRef.current.seekTo(currentTime / duration);
        }, 100);
    }, [currentWaveform]);

    useEffect(() => {
        if (!waveSurferRef.current || !duration) return;
        const ratio = currentTime / duration;
        waveSurferRef.current.seekTo(Math.min(Math.max(ratio, 0), 1));
    });

    useEffect(() => {
        if (!containerRef.current) return;
        const onResize = e => {
            e.preventDefault();
            const delta = Math.sign(e.deltaY);
            const newMinPxPerSec = Math.min(Math.max(minPxPerSec.current + delta * -5, 10), 800);
            if (newMinPxPerSec !== minPxPerSec.current) {
                minPxPerSec.current = newMinPxPerSec;
                setCurrentWaveform(getWaveformResolution(newMinPxPerSec));
            }
        };
        containerRef.current.addEventListener('wheel', onResize, { passive: false });
    }, [waveSurferInitialized]);

    return (
        <>
            <div style={{ width: '100%' }} ref={containerRef} />
            <div style={{ width: '100%' }} ref={timelineRef} />
        </>
    )
};
export default AudioTimeline;