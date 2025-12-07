import { Row } from 'antd';
import AudioTimeline from "./AudioTimeline";

const MainContent = () => {
    return (
        <Row style={rowStyle}>
            <AudioTimeline />
        </Row>
    )
};
export default MainContent;

const rowStyle = {
    margin: '20px'
};