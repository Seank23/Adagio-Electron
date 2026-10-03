import { useEffect, useRef, useState } from 'react';
import { useSelector } from 'react-redux';
import Styled from '@emotion/styled';
import RollingNotesHeatMap from './RollingNotesHeatMap';
import { MIN_FREQ } from '../constants';

const selectKeyHistogram = state => state.analysis.keyHistogram;
const selectChordHistogram = state => state.analysis.chordHistogram;

const AnalysisSection = () => {
    const detectedKey = useSelector(state => state.analysis.detectedKey);
    // Measured here until Stage 8 replaces this section: the spectrum no longer
    // publishes its width.
    const containerRef = useRef(null);
    const [canvasWidth, setCanvasWidth] = useState(1000);
    useEffect(() => {
        const container = containerRef.current;
        if (!container)
            return;
        const observer = new ResizeObserver(() => setCanvasWidth(container.clientWidth || 1000));
        observer.observe(container);
        return () => observer.disconnect();
    }, []);
    const spectrumSR = useSelector(state => state.analysis.spectrumSR);
    const showLogScale = useSelector(state => state.settings.showLogScale);
    const topChord = useSelector(state => state.analysis.predictedChords[0]?.name);

    return (
        <SectionContainer ref={containerRef}>
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