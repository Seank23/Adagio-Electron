import { useEffect, useRef, useState } from 'react';
import { theme, FloatButton, Card, Popover, Slider, Row } from 'antd';
import { PlayCircleFilled, PauseCircleFilled } from '@ant-design/icons';
import { FontAwesomeIcon } from '@fortawesome/react-fontawesome'
import { faVolumeHigh, faClockRotateLeft } from '@fortawesome/free-solid-svg-icons'
import Styled from '@emotion/styled';
import { selectIsFileOpen, selectIsPaused, selectIsPlaying } from '../store/playbackSlice';
import { setStatusMessage } from '../store/appSlice';
import { LIMITS } from '../utils/protocol';
import { useEngineCommands } from '../hooks/useEngineCommands';
import { useSelector, useDispatch } from 'react-redux';

const INITIAL_VOLUME_PERCENT = 20;

const toPercent = value => Math.round(value * 100);
const SPEED_MIN_PERCENT = toPercent(LIMITS.speedMin);
const SPEED_MAX_PERCENT = toPercent(LIMITS.speedMax);
const VOLUME_MIN_PERCENT = toPercent(LIMITS.volumeMin);
const VOLUME_MAX_PERCENT = toPercent(LIMITS.volumeMax);

const SLIDER_SEND_INTERVAL_MS = 50;

export const PlaybackControls = () => {
  const { token } = theme.useToken();
  const dispatch = useDispatch();
  const commands = useEngineCommands();

  const fileOpen = useSelector(selectIsFileOpen);
  const audioPlaying = useSelector(selectIsPlaying);
  const isPaused = useSelector(selectIsPaused);
  const engineSpeed = useSelector(state => state.playback.speed);
  const engineVolume = useSelector(state => state.playback.volume);
  const connectionState = useSelector(state => state.app.connectionState);

  const [volumePercent, setVolumePercent] = useState(INITIAL_VOLUME_PERCENT);
  const [speedPercent, setSpeedPercent] = useState(100);
  const draggingRef = useRef(null);
  const lastSendRef = useRef(0);

  const report = result => {
    if (result?.ok === false)
      dispatch(setStatusMessage({ type: 'error', message: result.error }));
  };

  useEffect(() => {
    if (connectionState !== 'connected')
      return;
    setVolumePercent(INITIAL_VOLUME_PERCENT);
    commands.setVolume(INITIAL_VOLUME_PERCENT / 100).then(report);
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [connectionState, commands]);

  // While a handle is held the user owns the value; otherwise the engine does. This is
  // what makes a new file's reset back to 100% show up on the speed slider.
  useEffect(() => {
    if (draggingRef.current !== 'speed')
      setSpeedPercent(toPercent(engineSpeed));
  }, [engineSpeed]);

  useEffect(() => {
    if (draggingRef.current !== 'volume')
      setVolumePercent(toPercent(engineVolume));
  }, [engineVolume]);

  const throttledSend = (kind, percent, send) => {
    draggingRef.current = kind;
    const now = Date.now();
    if (now - lastSendRef.current < SLIDER_SEND_INTERVAL_MS)
      return;
    lastSendRef.current = now;
    send(percent / 100).then(report);
  };

  const finishDrag = (percent, send) => {
    draggingRef.current = null;
    lastSendRef.current = 0;
    send(percent / 100).then(report);
  };

  const handlePlay = async () => report(await commands.play());
  const handlePause = async () => report(await commands.pause());

  const controlsPanel = (
    <Container>
      <Row>
        <SliderIcon icon={faVolumeHigh} />
        <PanelSlider
          min={VOLUME_MIN_PERCENT}
          max={VOLUME_MAX_PERCENT}
          value={volumePercent}
          tooltip={{ formatter: value => `Volume: ${value}%` }}
          onChange={value => { 
            setVolumePercent(value);
            throttledSend('volume', value, commands.setVolume);
          }}
          onChangeComplete={value => finishDrag(value, commands.setVolume)}
        />
      </Row>
      <Row>
        <SliderIcon icon={faClockRotateLeft} />
        <PanelSlider
          min={SPEED_MIN_PERCENT}
          max={SPEED_MAX_PERCENT}
          value={speedPercent}
          disabled={!fileOpen}
          tooltip={{ formatter: value => `Speed: ${value}%` }}
          onChange={value => { 
            setSpeedPercent(value);
            throttledSend('speed', value, commands.setSpeed);
          }}
          onChangeComplete={value => finishDrag(value, commands.setSpeed)}
        />
      </Row>
    </Container>
  );

  return (
    <Popover content={controlsPanel} placement="bottom">
      <Card style={{ borderRadius: '24px' }}>
        <FloatGroup shape="circle">
          <FloatButton
            onClick={() => handlePlay()}
            icon={<PlayCircleFilled style={{ fontSize: 32, color: audioPlaying ? token.colorPrimary : '' }} />}
            disabled={!fileOpen}
          />
          <FloatButton
            onClick={() => handlePause()}
            icon={<PauseCircleFilled style={{ fontSize: 32, color: isPaused ? token.colorPrimary : '' }} />}
            disabled={!fileOpen}
          />
        </FloatGroup>
      </Card>
    </Popover>
  )
};

const FloatGroup = Styled(FloatButton.Group)`
  position: relative;
  inset-inline-end: 0;
  bottom: 0;
  & > div {
    flex-direction: row;
  }
`;

const Container = Styled('div')`
  display: flex;
  flex-direction: column;
  align-items: center;
`;

const PanelSlider = Styled(Slider)`
  width: 150px;
  margin-right: 8px;
`;

const SliderIcon = Styled(FontAwesomeIcon)`
  padding-top: 10px;
  padding-right: 5px;
`;
