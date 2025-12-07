import { StrictMode } from 'react';
import { createRoot } from 'react-dom/client';
import "antd/dist/reset.css";
import App from './App.jsx';
import { WebSocketProvider } from '../../engine/WebSocketContext.jsx';
import { Provider } from 'react-redux';
import { store } from './store/Store.jsx';

createRoot(document.getElementById('root')).render(
  <StrictMode>
    <WebSocketProvider>
      <Provider store={store}>
        <App />
      </Provider>
    </WebSocketProvider>
  </StrictMode>,
);
