import Styled from '@emotion/styled';
import { useSelector } from 'react-redux';
import EngineEventRouter from './router/EngineEventRouter';
import TopBar from './components/TopBar';
import TransportBar from './components/TransportBar';
import StatusBar from './components/StatusBar';
import TimelinePanel from './components/TimelinePanel';
import SpectrumPanel from './components/SpectrumPanel';
import AnalysisSection from './components/AnalysisSection';
import EmptyState from './components/EmptyState';
import { Panel } from './components/Panel';
import { selectIsFileOpen } from './store/playbackSlice';
import { useKeyboardShortcuts } from './hooks/useKeyboardShortcuts';
import { useLoadFile } from './hooks/useOpenFile';
import { useFileDrop } from './hooks/useFileDrop';

const App = () => {
    const isFileOpen = useSelector(selectIsFileOpen);
    const loadFile = useLoadFile();
    const isDragging = useFileDrop({ onFile: loadFile });
    useKeyboardShortcuts();

    return (
        <>
            <EngineEventRouter />
            <Shell fileOpen={isFileOpen}>
                <TopBar />
                <TimelineGroup fileOpen={isFileOpen}>
                    <TransportBar />
                    {isFileOpen && <TimelinePanel />}
                </TimelineGroup>
                {isFileOpen
                    ? (
                        <Body>
                            <Main>
                                <SpectrumPanel />
                                <NoteActivitySlot>
                                    <AnalysisSection />
                                </NoteActivitySlot>
                            </Main>
                            <Panel />
                        </Body>
                    )
                    : <EmptyState isDragging={isDragging} />}
                <StatusBar />
            </Shell>
        </>
    );
};
export default App;

const withoutFileOpen = { shouldForwardProp: prop => prop !== 'fileOpen' };

// The second row's floor is the transport, a gap and the timeline's 120px minimum.
// With no file open, the second row is the transport alone and the empty state takes
// the rest.
const Shell = Styled('div', withoutFileOpen)`
    display: grid;
    grid-template-rows: ${({ fileOpen }) => fileOpen
        ? '44px minmax(185px, 1fr) minmax(0, 3fr) 26px'
        : '44px 64px minmax(0, 1fr) 26px'};
    height: 100vh;
    gap: 1px;
    overflow: hidden;
    background: var(--bg-app);
`;

// The transport and the timeline, together a quarter of the height between the bars.
// The transport keeps its height and the timeline takes the rest.
const TimelineGroup = Styled('div', withoutFileOpen)`
    display: grid;
    grid-template-rows: ${({ fileOpen }) => fileOpen ? '64px minmax(120px, 1fr)' : '64px'};
    gap: 1px;
    min-height: 0;
`;

const Body = Styled.div`
    display: grid;
    grid-template-columns: minmax(0, 1fr) 320px;
    gap: 1px;
    min-height: 0;
`;

const Main = Styled.div`
    display: grid;
    grid-template-rows: minmax(0, 3fr) minmax(0, 1fr);
    gap: 1px;
    min-width: 0;
    min-height: 0;
`;

const NoteActivitySlot = Styled(Panel)`
    padding: 0 12px 12px;
    overflow-y: auto;
`;
