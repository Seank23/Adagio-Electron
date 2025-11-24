import { Layout, theme } from 'antd';
import { AudioContext, useAudioProvider } from './context/AudioContext';
import HeaderBar from './components/HeaderBar';
import FooterBar from './components/FooterBar';

const { Header, Footer, Sider, Content } = Layout;

const App = () => {
  const [audioProvider] = useAudioProvider();
  const { token } = theme.useToken();

  return (
    <AudioContext.Provider value={audioProvider}>
      <Layout style={{ minHeight: '100vh' }}>
      <Header style={headerStyle(token)}><HeaderBar /></Header>
      <Content style={contentStyle(token)}>
      </Content>
      <Footer style={footerStyle(token)}><FooterBar /></Footer>
      </Layout>
    </AudioContext.Provider>
  )
}
export default App;

const headerStyle = theme => ({
  padding: 0,
  background: theme?.colorBgContainer
});
const contentStyle = theme => ({ 
  minHeight: '500px',
  background: theme?.colorBgContainer
});
const footerStyle = theme => ({
  height: '60px',
  background: theme?.colorBgContainer
});