import { Layout, theme } from 'antd';
import EngineEventRouter from './router/EngineEventRouter';
import HeaderBar from './components/HeaderBar';
import FooterBar from './components/FooterBar';
import MainContent from './components/MainContent';

const { Header, Footer, Content } = Layout;

const App = () => {
  const { token } = theme.useToken();

  return (
    <>
    <EngineEventRouter />
    <Layout style={{ minHeight: '100vh' }}>
      <Header style={headerStyle(token)}><HeaderBar /></Header>
      <Content style={contentStyle(token)}>
        <MainContent />
      </Content>
      <Footer style={footerStyle(token)}><FooterBar /></Footer>
    </Layout>
    </>
  )
}
export default App;

const headerStyle = theme => ({
  height: '111px',
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