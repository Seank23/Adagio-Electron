import { useContext } from 'react';
import { AudioContext } from '../context/AudioContext';
import { PlaybackControls } from './PlaybackControls';
import { Button, Row, Col } from 'antd';

const HeaderBar = () => {
    const { fileOpen, setFileOpen, audioPlaying, setStatus } = useContext(AudioContext);

    const handleOpenCloseFile = async () => {
    if (fileOpen) {
      const result = await window.api.clear();
      setFileOpen(false);
      setStatus(result.status);
    } else {
      const file = await window.api.selectAudioFile();
      setStatus({
        type: 'loading',
        message: 'Loading audio...'
      });
      const result = await window.api.load(file);
      setFileOpen(true);
      setStatus(result.status);
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