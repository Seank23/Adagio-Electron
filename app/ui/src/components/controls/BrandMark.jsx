import Styled from '@emotion/styled';

const BrandMark = ({ size = 20 }) => (
    <Mark aria-hidden="true" style={{ width: size, height: size, borderRadius: size / 4, fontSize: size * 0.6 }}>A</Mark>
);
export default BrandMark;

const Mark = Styled.span`
    display: inline-flex;
    align-items: center;
    justify-content: center;
    flex-shrink: 0;
    background: var(--accent-primary);
    color: var(--text-on-accent);
    font-family: var(--font-ui);
    font-weight: 700;
    line-height: 1;
`;
