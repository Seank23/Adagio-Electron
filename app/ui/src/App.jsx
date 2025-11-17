import { useState } from 'react';
import reactLogo from './assets/react.svg';
import viteLogo from '/vite.svg';
import './App.css';

const App = () => {
  const [status, setStatus] = useState('No file loaded');

  const handleOpenFile = async () => {
    const file = await window.api.selectAudioFile();
    console.log("File loaded:", file);
    const result = await window.api.load(file);
    setStatus(result.status);
  };

  const handlePlay = async () => {
    const result = await window.api.play();
    setStatus(result.status);
  };

  const handlePause = async () => {
    const result = await window.api.pause();
    setStatus(result.status);
  };

  return (
    <>
      <div>
        <a href="https://vite.dev" target="_blank">
          <img src={viteLogo} className="logo" alt="Vite logo" />
        </a>
        <a href="https://react.dev" target="_blank">
          <img src={reactLogo} className="logo react" alt="React logo" />
        </a>
      </div>
      <h1>Vite + React</h1>
      <div className="card">
        <button onClick={() => handleOpenFile()}>Open Audio File</button>
        <button onClick={() => handlePlay()}>Play</button>
        <button onClick={() => handlePause()}>Pause</button>
        <p>{status}</p>
      </div>
      <p className="read-the-docs">
        Click on the Vite and React logos to learn more
      </p>
    </>
  )
}

export default App;
