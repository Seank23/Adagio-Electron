import { useEffect, useState } from 'react';
import Styled from '@emotion/styled';
import { css, keyframes } from '@emotion/react';
import { useDispatch, useSelector } from 'react-redux';
import { Tooltip } from 'antd';
import { AlertCircle, CheckCircle2, Info, Loader2 } from 'lucide-react';
import { Panel, Spacer } from './Panel';
import { resetStatus } from '../store/appSlice';
import { selectIsFileOpen } from '../store/playbackSlice';
import { getLatestSpectrum, getSpectrumRate } from '../engine-client/FrameStore';

const MESSAGE_LIFETIME_MS = 3000;
const RATE_POLL_MS = 1000;

const StatusBar = () => {
    const isFileOpen = useSelector(selectIsFileOpen);

    return (
        <Bar>
            <StatusMessage />
            <Spacer />
            {isFileOpen && <AnalysisInfo />}
            <EngineIndicator />
        </Bar>
    );
};
export default StatusBar;

const ICONS = {
    error: { Icon: AlertCircle, colour: 'var(--status-error)' },
    success: { Icon: CheckCircle2, colour: 'var(--status-ok)' },
    loading: { Icon: Loader2, colour: 'var(--accent-primary-fg)', spin: true },
    info: { Icon: Info, colour: 'var(--text-secondary)' },
};

const READY_MESSAGE = 'Ready. Open or drop a file to begin';

const StatusMessage = () => {
    const dispatch = useDispatch();
    const status = useSelector(state => state.app.statusMessage);
    const isFileOpen = useSelector(selectIsFileOpen);
    const isConnected = useSelector(state => state.app.connectionState === 'connected');

    // Everything but a load in progress clears itself.
    useEffect(() => {
        if (!status?.type || status.type === 'loading')
            return;
        const timeout = setTimeout(() => dispatch(resetStatus()), MESSAGE_LIFETIME_MS);
        return () => clearTimeout(timeout);
    }, [status, dispatch]);

    if (!status?.message) {
        if (isFileOpen || !isConnected)
            return null;
        return <Message><MessageText>{READY_MESSAGE}</MessageText></Message>;
    }

    const icon = ICONS[status.type];
    return (
        <Message>
            {icon && (
                <MessageIcon style={{ color: icon.colour }} spin={icon.spin}>
                    <icon.Icon size={13} />
                </MessageIcon>
            )}
            <MessageText>{status.message}</MessageText>
        </Message>
    );
};

const AnalysisInfo = () => {
    const sampleRate = useSelector(state => state.analysis.spectrumSR);
    const [rate, setRate] = useState(0);
    const [fftSize, setFftSize] = useState(0);

    useEffect(() => {
        const poll = () => {
            setRate(Math.round(getSpectrumRate()));
            setFftSize((getLatestSpectrum()?.count ?? 0) * 2);
        };
        poll();
        const interval = setInterval(poll, RATE_POLL_MS);
        return () => clearInterval(interval);
    }, []);

    const parts = [`Analysis ${rate} frames/s`];
    if (sampleRate > 0)
        parts.push(`${Number((sampleRate / 1000).toFixed(1))} kHz`);
    if (fftSize > 0)
        parts.push(`${fftSize} pt`);

    return (
        <Tooltip title={<ExecutionTime />} mouseEnterDelay={0.5}>
            <AnalysisText>{parts.join(' · ')}</AnalysisText>
        </Tooltip>
    );
};

// Only rendered while the tooltip is open.
const ExecutionTime = () => {
    const executionTime = useSelector(state => state.analysis.executionTime);
    return `Pipeline time: ${Number(executionTime ?? 0).toFixed(2)} ms per frame`;
};

const EngineIndicator = () => {
    const connectionState = useSelector(state => state.app.connectionState);
    const engineStatus = useSelector(state => state.app.engineStatus);
    // The socket says whether the engine is reachable; the spawn status says why it is
    // not. A failed spawn is worth showing even while the socket is still retrying.
    const connection = describeConnection(connectionState, engineStatus);

    return (
        <Tooltip title={connection.title} mouseEnterDelay={0.3}>
            <Engine role="status">
                <Dot style={{ background: connection.colour }} />
                <EngineLabel>{connection.label}</EngineLabel>
            </Engine>
        </Tooltip>
    );
};

const describeConnection = (connectionState, engineStatus) => {
    if (connectionState === 'connected')
        return { colour: 'var(--status-ok)', label: 'Engine connected', title: 'Engine connected' };

    const failed = engineStatus?.state === 'failed' || engineStatus?.state === 'exited';
    if (failed)
        return { colour: 'var(--status-error)', label: 'Engine unavailable', title: engineStatus.error || 'Engine unavailable' };

    if (connectionState === 'disconnected')
        return { colour: 'var(--status-error)', label: 'Disconnected', title: 'Disconnected. Reconnecting to the engine...' };

    return { colour: 'var(--status-warning)', label: 'Connecting', title: 'Connecting to the engine...' };
};

const Bar = Styled(Panel)`
    flex-direction: row;
    align-items: center;
    gap: 16px;
    padding: 0 12px;
`;

const Message = Styled.div`
    display: flex;
    align-items: center;
    gap: 6px;
    min-width: 0;
`;

const rotate = keyframes`
    to { transform: rotate(360deg); }
`;

const MessageIcon = Styled('span', { shouldForwardProp: prop => prop !== 'spin' })`
    display: inline-flex;
    flex-shrink: 0;
    ${({ spin }) => spin && css`animation: ${rotate} 1s linear infinite;`}
`;

const MessageText = Styled.span`
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
    font: 400 11px/1 var(--font-ui);
    color: var(--text-secondary);
`;

const AnalysisText = Styled.span`
    flex-shrink: 0;
    white-space: nowrap;
    font: 400 10px/1 var(--font-mono);
    color: var(--text-muted);
`;

const Engine = Styled.div`
    display: flex;
    align-items: center;
    flex-shrink: 0;
    gap: 6px;
`;

const Dot = Styled.span`
    width: 7px;
    height: 7px;
    border-radius: 50%;
`;

const EngineLabel = Styled.span`
    font: 400 11px/1 var(--font-ui);
    color: var(--text-secondary);
`;
