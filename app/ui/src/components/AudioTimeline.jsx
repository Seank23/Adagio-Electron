import { useEffect, useRef } from 'react';
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

    const handleSeek = time => {
        dispatch(setCurrentTime(time));
        window.api.seek(time);
    }

    useEffect(() => {
        if (!containerRef.current || duration === 0 || !waveformData) return;

        let lowRes = waveformData?.find(data => data.resolution === 8192)?.peaks || null;
        lowRes = lowRes?.map(point => point.max)?.flat();
        console.log(lowRes);
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
        waveSurfer.load(null, lowRes, duration);
        waveSurfer.on('interaction', time => handleSeek(time));
        waveSurferRef.current = waveSurfer;

        return () => {
            waveSurfer.destroy();
        };
    }, [waveformData, duration]);

    useEffect(() => {
        if (!waveSurferRef.current || !duration) return;
        const ratio = currentTime / duration;
        waveSurferRef.current.seekTo(Math.min(Math.max(ratio, 0), 1));
    });

    return (
        <>
            <div style={{ width: '100%' }} ref={containerRef} />
            <div style={{ width: '100%' }} ref={timelineRef} />
        </>
    )
};
export default AudioTimeline;