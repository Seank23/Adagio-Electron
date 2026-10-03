import Styled from '@emotion/styled';
import { Slider } from 'antd';

// An icon, a 112px slider and its value.
const EngineSlider = ({ icon, label, min, max, percent, onChange, onChangeComplete, disabled = false }) => (
    <Row>
        <Icon aria-hidden>{icon}</Icon>
        <Track
            min={min}
            max={max}
            value={percent}
            disabled={disabled}
            tooltip={{ open: false }}
            aria-label={label}
            onChange={onChange}
            onChangeComplete={onChangeComplete}
        />
        <Value>{percent}%</Value>
    </Row>
);
export default EngineSlider;

const Row = Styled.div`
    display: flex;
    align-items: center;
    gap: 10px;
`;

const Icon = Styled.span`
    display: inline-flex;
    color: var(--text-secondary);
`;

const Track = Styled(Slider)`
    width: 112px;
    margin: 0;
`;

const Value = Styled.span`
    width: 34px;
    text-align: right;
    font: 500 11px/1 var(--font-mono);
    font-variant-numeric: tabular-nums;
    color: var(--text-primary);
`;
