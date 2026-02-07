import { Row } from 'antd';
import AudioTimeline from "./AudioTimeline";
import SpectrumCanvas from './SpectrumCanvas';

const MainContent = () => {
    return (
        <Row style={rowStyle}>
            <AudioTimeline />
            <SpectrumCanvas />
        </Row>
    )
};
export default MainContent;

const rowStyle = {
    margin: '20px',
    display: 'flex',
    flexDirection: 'column',
};