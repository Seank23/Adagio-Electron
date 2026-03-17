import { useSelector } from 'react-redux';
import Styled from '@emotion/styled';
import RollingNotesHeatMap from './RollingNotesHeatMap';
import { MIN_FREQ } from '../constants';

const AnalysisSection = () => {
    const detectedKey = useSelector(state => state.analysis.detectedKey);
    const keyHistogram = useSelector(state => state.analysis.keyHistogram);
    const chordHistogram = useSelector(state => state.analysis.chordHistogram);
    const canvasWidth = useSelector(state => state.app.canvasWidth);
    const spectrumSR = useSelector(state => state.analysis.spectrumSR);
    const showLogScale = useSelector(state => state.settings.showLogScale);
    const predictedChords = useSelector(state => state.analysis.predictedChords);

    const SectionContainer = Styled('div')`
        margin-bottom: 20px;
    `;

    const SummaryBar = Styled('div')`
        display: flex;
        justify-content: space-between;
        align-items: center;
        margin-bottom: 10px;
    `;

    return (
        <SectionContainer>
            <SummaryBar>
                <h3>{predictedChords[0]?.name || 'N/A'}</h3>
                <h3>Detected Key: {detectedKey || 'N/A'}</h3>
            </SummaryBar>
            <RollingNotesHeatMap
                histogram={keyHistogram}
                width={canvasWidth}
                minFreq={MIN_FREQ}
                maxFreq={spectrumSR / 2}
                showLogScale={showLogScale}
            />
            <RollingNotesHeatMap
                histogram={chordHistogram}
                width={canvasWidth}
                minFreq={MIN_FREQ}
                maxFreq={spectrumSR / 2}
                showLogScale={showLogScale}
            />
        </SectionContainer>
    );
};

export default AnalysisSection;