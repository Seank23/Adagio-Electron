import { StrictMode, Profiler } from 'react';
import { createRoot } from 'react-dom/client';
import "antd/dist/reset.css";
// Bundled rather than fetched from Google Fonts, which fails offline and in the packaged app.
import '@fontsource/inter/400.css';
import '@fontsource/inter/500.css';
import '@fontsource/inter/600.css';
import '@fontsource/inter/700.css';
import '@fontsource/jetbrains-mono/400.css';
import '@fontsource/jetbrains-mono/500.css';
import '@fontsource/jetbrains-mono/700.css';
import App from './App.jsx';
import { WebSocketProvider } from './engine-client/WebSocketProvider.jsx';
import { Provider } from 'react-redux';
import { store } from './store/Store.jsx';
import AppThemeProvider from './theme/AppThemeProvider.jsx';
import { startDevPerf, recordCommit } from './utils/devPerf.jsx';

startDevPerf();

createRoot(document.getElementById('root')).render(
  <StrictMode>
    <WebSocketProvider>
      <Provider store={store}>
        <AppThemeProvider>
          <Profiler id="App" onRender={recordCommit}>
            <App />
          </Profiler>
        </AppThemeProvider>
      </Provider>
    </WebSocketProvider>
  </StrictMode>,
);
