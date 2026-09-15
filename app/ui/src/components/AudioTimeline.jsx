import { useEffect, useRef } from 'react';
import { useSelector, useDispatch } from 'react-redux';
import { theme } from 'antd';
import { setCurrentTime } from '../store/playbackSlice';
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

    const minPxPerSec = useRef(1);
    const currentWaveformRef = useRef(null);
    const waveformBusyRef = useRef(false);

    const handleSeek = time => {
        dispatch(setCurrentTime(time));
        window.api.seek(time);
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
            waveColor: '#666',
            progressColor: token.colorPrimary,
            cursorColor: token.colorPrimaryActive,
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