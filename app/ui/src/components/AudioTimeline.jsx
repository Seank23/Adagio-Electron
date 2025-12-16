import { Slider } from 'antd';
import { useSelector, useDispatch } from 'react-redux';
import { setCurrentSample } from '../store/PlaybackSlice';
import Styled from '@emotion/styled';

const AudioTimeline = () => {
    const dispatch = useDispatch();
    const currentSample = useSelector(state => state.playback.currentSample);
    const totalSamples = useSelector(state => state.playback.totalSamples);

    const handleSeek = value => {
        const newSample = parseFloat(value);
        dispatch(setCurrentSample(newSample));
        window.api.seek(newSample);
    }

    const Timeline = Styled(Slider)`
    width: 100%;
  `;

    return (
        <>
            <Timeline min={0} max={totalSamples} value={currentSample} onChange={handleSeek} />
        </>
    )
};
export default AudioTimeline;