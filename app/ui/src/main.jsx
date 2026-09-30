import { StrictMode, Profiler } from 'react';
import { createRoot } from 'react-dom/client';
import "antd/dist/reset.css";
import App from './App.jsx';
import { WebSocketProvider } from './engine-client/WebSocketProvider.jsx';
import { Provider } from 'react-redux';
import { store } from './store/Store.jsx';
import { startDevPerf, recordCommit } from './utils/devPerf.jsx';

startDevPerf();

createRoot(document.getElementById('root')).render(
  <StrictMode>
    <WebSocketProvider>
      <Provider store={store}>
        <Profiler id="App" onRender={recordCommit}>
          <App />
        </Profiler>
      </Provider>
    </WebSocketProvider>
  </StrictMode>,
);
