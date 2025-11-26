import { StrictMode } from 'react';
import { createRoot } from 'react-dom/client';
import "antd/dist/reset.css";
import App from './App.jsx';
import { WebSocketProvider } from '../../engine/WebSocketContext.jsx';

createRoot(document.getElementById('root')).render(
  <StrictMode>
    <WebSocketProvider>
      <App />
    </WebSocketProvider>
  </StrictMode>,
);
