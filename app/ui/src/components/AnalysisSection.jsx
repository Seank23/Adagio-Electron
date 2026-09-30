import { useSelector } from 'react-redux';
import Styled from '@emotion/styled';
import RollingNotesHeatMap from './RollingNotesHeatMap';
import { MIN_FREQ } from '../constants';

const selectKeyHistogram = state => state.analysis.keyHistogram;
const selectChordHistogram = state => state.analysis.chordHistogram;

const AnalysisSection = () => {
    const detectedKey = useSelector(state => state.analysis.detectedKey);
    const canvasWidth = useSelector(state => state.app.canvasWidth);
    const spectrumSR = useSelector(state => state.analysis.spectrumSR);
    const showLogScale = useSelector(state => state.settings.showLogScale);
    const topChord = useSelector(state => state.analysis.predictedChords[0]?.name);

    return (
        <SectionContainer>
            <SummaryBar>
                <h3>{topChord || 'N/A'}</h3>
                <h3>Detected Key: {detectedKey || 'N/A'}</h3>
            </SummaryBar>
            <RollingNotesHeatMap
                histogramSelector={selectKeyHistogram}
                width={canvasWidth}
                minFreq={MIN_FREQ}
                maxFreq={spectrumSR / 2}
                showLogScale={showLogScale}
            />
            <RollingNotesHeatMap
                histogramSelector={selectChordHistogram}
                width={canvasWidth}
                minFreq={MIN_FREQ}
                maxFreq={spectrumSR / 2}
                showLogScale={showLogScale}
            />
        </SectionContainer>
    );
};
export default AnalysisSection;

const SectionContainer = Styled('div')`
    margin-bottom: 20px;
`;

const SummaryBar = Styled('div')`
    display: flex;
    justify-content: space-between;
    align-items: center;
    margin-bottom: 10px;
`;