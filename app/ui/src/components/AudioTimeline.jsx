import { Slider } from 'antd';
import { useSelector } from 'react-redux';
import Styled from '@emotion/styled';

const AudioTimeline = () => {
    const currentSample = useSelector(state => state.playback.currentSample);
    const totalSamples = useSelector(state => state.playback.totalSamples);

    const Timeline = Styled(Slider)`
    width: 100%;
  `;

    return (
        <>
            <Timeline max={totalSamples} value={currentSample} />
        </>
    )
};
export default AudioTimeline;