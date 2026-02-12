import { Row, Checkbox } from 'antd';
import AudioTimeline from "./AudioTimeline";
import SpectrumCanvas from './SpectrumCanvas';
import Styled from '@emotion/styled';
import { setShowLogScale } from '../store/settingsSlice';
import { useDispatch, useSelector } from 'react-redux';

const MainContent = () => {
    const dispatch = useDispatch();
    const showLogScale = useSelector(state => state.settings.showLogScale);
    const isFileOpen = useSelector(state => state.app.isFileOpen);

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
                {isFileOpen && (
                    <div>
                        <LogCheckbox onChange={(e) => dispatch(setShowLogScale(e.target.checked))} checked={showLogScale}>Show Log Scale</LogCheckbox>
                    </div>
                )}
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