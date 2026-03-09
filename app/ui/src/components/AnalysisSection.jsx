import { useSelector } from 'react-redux';
import Styled from '@emotion/styled';

const AnalysisSection = () => {
    const detectedKey = useSelector(state => state.analysis.detectedKey);

    const SectionContainer = Styled('div')`
        margin-top: 20px;
    `;

    return (
        <SectionContainer>
            <h3>Detected Key: {detectedKey || 'N/A'}</h3>
        </SectionContainer>
    );
};

export default AnalysisSection;