import { useState } from 'react';
import reactLogo from './assets/react.svg';
import viteLogo from '/vite.svg';
import './App.css';

const App = () => {
  const [status, setStatus] = useState('No file loaded');
  const [fileOpen, setFileOpen] = useState(false);
  const [audioPlaying, setAudioPlaying] = useState(false);

  const handleOpenCloseFile = async () => {
    if (fileOpen) {
      const result = await window.api.clear();
      setFileOpen(false);
      setStatus(result.status);
    } else {
      const file = await window.api.selectAudioFile();
      const result = await window.api.load(file);
      setFileOpen(true);
      setStatus(result.status);
    }
  };

  const handlePlayPause = async () => {
    if (audioPlaying) {
      const result = await window.api.pause();
      setAudioPlaying(false);
      setStatus(result.status);
    } else {
      const result = await window.api.play();
      setAudioPlaying(true);
      setStatus(result.status);
    }
  };

  return (
    <>
      <div className="card">
        <button onClick={() => handleOpenCloseFile()} disabled={audioPlaying}>{fileOpen ? 'Close Audio File' : 'Open Audio File'}</button>
        <button onClick={() => handlePlayPause()} disabled={!fileOpen}>{audioPlaying ? 'Pause' : 'Play'}</button>
        <p>{status}</p>
      </div>
    </>
  )
}

export default App;
