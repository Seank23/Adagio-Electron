import Styled from '@emotion/styled';
import logo from '@asset/Adagio_Logo.png';

const BrandMark = ({ size = 20 }) => (
    <Logo src={logo} alt="" aria-hidden="true" width={size} height={size} draggable={false} />
);
export default BrandMark;

const Logo = Styled.img`
    display: block;
    flex-shrink: 0;
    user-select: none;
`;
