import { PlaybackControls } from './PlaybackControls';
import { Button, Row, Col } from 'antd';
import { useSelector, useDispatch } from 'react-redux';
import { setStatusMessage } from '../store/appSlice';

const HeaderBar = () => {
  const dispatch = useDispatch();
  const fileOpen = useSelector(state => state.app.isFileOpen);
  const audioPlaying = useSelector(state => state.playback.isPlaying);

    const handleOpenCloseFile = async () => {
    if (fileOpen) {
      await window.api.clear();
    } else {
      const file = await window.api.selectAudioFile();
      dispatch(setStatusMessage({ type: 'loading', message: 'Loading audio...' }));
      await window.api.load(file);
    }
  };

  return (
    <Row>
      <Col style={colStyle} span={8}>
        <Button style={openFileStyle} onClick={() => handleOpenCloseFile()} disabled={audioPlaying}>
          {fileOpen ? 'Close Audio File' : 'Open Audio File'}
        </Button>
      </Col>
      <Col style={{...colStyle, justifyContent: 'center'}} span={8}>
        <PlaybackControls />
      </Col>
    </Row>
  );
}
export default HeaderBar;

const colStyle = {
  display: 'flex',
  padding: '10px',
}
const openFileStyle = {
  width: '200px'
};