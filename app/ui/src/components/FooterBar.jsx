import { useEffect, useRef } from 'react';
import { useSelector, useDispatch } from 'react-redux';
import { resetStatus } from '../store/appSlice'; 
import Styled from '@emotion/styled';
import { theme } from 'antd';
import { ExclamationCircleOutlined, CheckCircleOutlined, LoadingOutlined, InfoCircleOutlined } from '@ant-design/icons';

const FooterBar = () => {
    const { token } = theme.useToken();
    const dispatch = useDispatch();
    const status = useSelector(state => state.app.statusMessage);
    const statusTimeout = useRef(null);

    useEffect(() => {
        clearTimeout(statusTimeout.current);
        if (status?.type && status.type !== 'loading') {
            statusTimeout.current = setTimeout(() => dispatch(resetStatus()), 3000);
        }
    }, [status]);

    const StatusDiv = Styled.div`
        display: flex;
        justify-content: end;
    `;

    const iconMap = {
        'error': <ExclamationCircleOutlined style={{ color: token.colorError, marginRight: '8px' }} />,
        'success': <CheckCircleOutlined style={{ color: token.colorSuccess, marginRight: '8px' }} />,
        'loading': <LoadingOutlined style={{ color: token.colorPrimary, marginRight: '8px' }} />,
        'info': <InfoCircleOutlined style={{ marginRight: '8px' }} />,
        undefined: null,
    };

    return (
        <StatusDiv>{status?.type && iconMap[status.type]}{status?.message && status.message}</StatusDiv>
    );
};
export default FooterBar;
