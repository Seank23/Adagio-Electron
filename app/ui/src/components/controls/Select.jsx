import Styled from '@emotion/styled';
import { Select as AntSelect } from 'antd';
import { ChevronDown } from 'lucide-react';

// antd's Select in the app's look: the colours come from AppThemeProvider's Select tokens, and the
// popup's border from here. fill makes it as wide as its row; otherwise it is as wide as its value.
// Its focus lands on an input, so the global shortcuts leave its keys alone.
const Select = ({ label, fill = false, styles, ...rest }) => (
    <AntSelect
        aria-label={label}
        suffixIcon={<Chevron aria-hidden><ChevronDown size={16} /></Chevron>}
        popupMatchSelectWidth={fill}
        styles={{
            root: { width: fill ? '100%' : undefined, fontWeight: 500 },
            popup: { root: { border: '1px solid var(--border-strong)' }, ...styles?.popup },
        }}
        {...rest}
    />
);
export default Select;

const Chevron = Styled.span`
    display: inline-flex;
    color: var(--text-secondary);
    pointer-events: none;
`;
