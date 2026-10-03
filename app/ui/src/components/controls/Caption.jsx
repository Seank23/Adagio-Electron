import Styled from '@emotion/styled';

// The small uppercase label above or beside a control: SPEED, POSITION.
const Caption = Styled.span`
    font: 600 9px/1 var(--font-ui);
    letter-spacing: .8px;
    text-transform: uppercase;
    color: var(--text-muted);
    white-space: nowrap;
`;
export default Caption;
