import { useEffect, useRef } from 'react';
import { useSelector, useDispatch } from 'react-redux';
import { resetStatus } from '../store/appSlice';
import Styled from '@emotion/styled';
import { theme, Tooltip } from 'antd';
import { ExclamationCircleOutlined, CheckCircleOutlined, LoadingOutlined, InfoCircleOutlined } from '@ant-design/icons';

const FooterBar = () => {
    const { token } = theme.useToken();
    const dispatch = useDispatch();
    const status = useSelector(state => state.app.statusMessage);
    const connectionState = useSelector(state => state.app.connectionState);
    const engineStatus = useSelector(state => state.app.engineStatus);
    const excutionTime = useSelector(state => state.analysis.executionTime);
    const isPlaying = useSelector(state => state.playback.isPlaying);
    const statusTimeout = useRef(null);

    useEffect(() => {
        clearTimeout(statusTimeout.current);
        if (status?.type && status.type !== 'loading') {
            statusTimeout.current = setTimeout(() => dispatch(resetStatus()), 3000);
        }
    }, [status]);

    const Container = Styled.div`
        display: flex;
        flex-direction: row;
        justify-content: space-between;
    `;

    const iconMap = {
        'error': <ExclamationCircleOutlined style={{ color: token.colorError, marginRight: '8px' }} />,
        'success': <CheckCircleOutlined style={{ color: token.colorSuccess, marginRight: '8px' }} />,
        'loading': <LoadingOutlined style={{ color: token.colorPrimary, marginRight: '8px' }} />,
        'info': <InfoCircleOutlined style={{ marginRight: '8px' }} />,
        undefined: null,
    };

    // The socket says whether the engine is reachable; the spawn status says why it is
    // not. A failed spawn is worth showing even while the socket is still retrying.
    const connection = describeConnection(connectionState, engineStatus, token);

    return (
        <Container>
            <div>{isPlaying && `Execution time: ${Number(excutionTime ?? 0).toFixed(2)}ms`}</div>
            <div style={rightGroupStyle}>
                <div>{status?.type && iconMap[status.type]}{status?.message && status.message}</div>
                <Tooltip title={connection.title}>
                    <span style={dotWrapStyle} role="img" aria-label={connection.title}>
                        <span style={{ ...dotStyle, background: connection.colour }} />
                    </span>
                </Tooltip>
            </div>
        </Container>
    );
};
export default FooterBar;

// The dot carries no label, so the title has to say the whole thing in every state.
const describeConnection = (connectionState, engineStatus, token) => {
    if (connectionState === 'connected')
        return { colour: token.colorSuccess, title: 'Engine connected' };

    const failed = engineStatus?.state === 'failed' || engineStatus?.state === 'exited';
    if (failed)
        return { colour: token.colorError, title: engineStatus.error || 'Engine unavailable' };

    if (connectionState === 'disconnected')
        return { colour: token.colorError, title: 'Disconnected. Reconnecting to the engine...' };

    return { colour: token.colorWarning, title: 'Connecting to the engine...' };
};

const rightGroupStyle = {
    display: 'flex',
    alignItems: 'center',
    gap: '12px',
};
// Padding only: it widens the hover target for the tooltip without moving the dot off
// the right edge.
const dotWrapStyle = {
    display: 'inline-flex',
    alignItems: 'center',
    padding: '4px',
};
const dotStyle = {
    width: '8px',
    height: '8px',
    borderRadius: '50%',
};
