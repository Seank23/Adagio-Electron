import { PlaybackControls } from './PlaybackControls';
import { Button, Row, Col } from 'antd';
import { useSelector, useDispatch } from 'react-redux';
import { setStatusMessage } from '../store/appSlice';
import { selectIsFileOpen, selectIsPlaying } from '../store/playbackSlice';
import { useEngineCommands } from '../hooks/useEngineCommands';

const HeaderBar = () => {
  const dispatch = useDispatch();
  const commands = useEngineCommands();
  const fileOpen = useSelector(selectIsFileOpen);
  const audioPlaying = useSelector(selectIsPlaying);

  const reportIfFailed = result => {
    if (result?.ok === false)
      dispatch(setStatusMessage({ type: 'error', message: result.error }));
  };

  const handleOpenCloseFile = async () => {
    if (fileOpen) {
      reportIfFailed(await commands.clear());
      return;
    }

    const file = await window.api.selectAudioFile();
    if (!file)
      return;

    dispatch(setStatusMessage({ type: 'loading', message: 'Loading audio...' }));
    reportIfFailed(await commands.load(file));
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