/* eslint-disable react-hooks/exhaustive-deps */
import { useEffect, useRef } from 'react';
import { theme, FloatButton, Card, Popover, Slider, Row } from 'antd';
import { PlayCircleFilled, PauseCircleFilled } from '@ant-design/icons';
import { FontAwesomeIcon } from '@fortawesome/react-fontawesome'
import { faVolumeHigh, faClockRotateLeft } from '@fortawesome/free-solid-svg-icons'
import Styled from '@emotion/styled';
import { setIsPlaying, setIsStarted } from '../store/PlaybackSlice';
import { useSelector, useDispatch } from 'react-redux';

export const PlaybackControls = () => {
  const { token } = theme.useToken();
  const fileOpen = useSelector(state => state.app.isFileOpen);
  const audioStarted = useSelector(state => state.playback.isStarted);
  const audioPlaying = useSelector(state => state.playback.isPlaying);
  const dispatch = useDispatch();
  const initialVolume = 20;

  useEffect(() => {
    window.api.changeVolume(initialVolume);
  }, []);
  
  useEffect(() => {
    if (!fileOpen)
      dispatch(setIsStarted(false));
    else if (fileOpen && audioPlaying)
      dispatch(setIsStarted(true));
  }, [audioStarted, fileOpen, audioPlaying]);
  const isPaused = audioStarted && !audioPlaying;

  const handlePlay = async () => {
    if (!audioPlaying) {
      await window.api.play();
      dispatch(setIsPlaying(true));
    }
  };

  const handlePause = async () => {
    if (audioPlaying) {
      await window.api.pause();
      dispatch(setIsPlaying(false));
    }
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

  const controlsPanel = useRef(
    <Container>
      <Row>
        <SliderIcon icon={faVolumeHigh} />
        <PanelSlider
          min={0}
          max={100}
          defaultValue={initialVolume}
          tooltip={{ formatter: value => `Volume: ${value}%` }}
          onChange={async value => await window.api.changeVolume(value)}
        />
      </Row>
      <Row>
        <SliderIcon icon={faClockRotateLeft} />
        <PanelSlider
          min={20}
          max={100}
          defaultValue={100}
          tooltip={{ formatter: value => `Speed: ${value}%` }}
        />
      </Row>
    </Container>
  );

  return (
    <Popover content={controlsPanel.current} placement="bottom">
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