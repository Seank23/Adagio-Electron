import Styled from '@emotion/styled';

// A 1px vertical rule between groups of controls.
const Divider = Styled('span', { shouldForwardProp: prop => prop !== 'height' })`
    flex-shrink: 0;
    width: 1px;
    height: ${({ height = 28 }) => height}px;
    background: var(--border-strong);
`;
export default Divider;
