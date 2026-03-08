import { Row, Checkbox } from 'antd';
import AudioTimeline from "./AudioTimeline";
import SpectrumCanvas from './SpectrumCanvas';
import Styled from '@emotion/styled';

const MainContent = () => {

    const DividerBar = Styled('div')`
        height: 100px;
        width: 100%;
    `;

    const LogCheckbox = Styled(Checkbox)`
        margin-left: 20px;
        margin-top: 60px;
    `;

    return (
        <Row style={rowStyle}>
            <AudioTimeline />
            <DividerBar>
            </DividerBar>
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