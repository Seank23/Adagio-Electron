import { Row, Checkbox } from 'antd';
import AudioTimeline from "./AudioTimeline";
import SpectrumCanvas from './SpectrumCanvas';
import AnalysisSection from './AnalysisSection';
import Styled from '@emotion/styled';
import { useSelector } from 'react-redux';

const MainContent = () => {
    const isFileOpen = useSelector(state => state.app.isFileOpen);

    const DividerBar = Styled('div')`
        height: 50px;
        width: 100%;
    `;

    return (
        <Row style={rowStyle}>
            <AudioTimeline />
            <DividerBar />
            {isFileOpen && <AnalysisSection />}
            {isFileOpen && <SpectrumCanvas />}
        </Row>
    )
};
export default MainContent;

const rowStyle = {
    margin: '20px',
    display: 'flex',
    flexDirection: 'column',
};