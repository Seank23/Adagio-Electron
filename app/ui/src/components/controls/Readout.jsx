import Styled from '@emotion/styled';
import Caption from './Caption';

const Readout = ({ caption, muted = false, children }) => (
    <Column>
        <Caption>{caption}</Caption>
        <Value muted={muted}>{children}</Value>
    </Column>
);
export default Readout;

const Column = Styled.div`
    display: flex;
    flex-direction: column;
    gap: 2px;
`;

const Value = Styled('span', { shouldForwardProp: prop => prop !== 'muted' })`
    font: 500 15px/1.2 var(--font-mono);
    font-variant-numeric: tabular-nums;
    color: ${({ muted }) => muted ? 'var(--text-secondary)' : 'var(--text-primary)'};
`;
