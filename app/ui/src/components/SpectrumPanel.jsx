import Styled from '@emotion/styled';
import { useDispatch, useSelector } from 'react-redux';
import { Panel, PanelHeader, Spacer } from './Panel';
import Chip from './controls/Chip';
import Caption from './controls/Caption';
import SpectrumCanvas from './SpectrumCanvas';
import TuningControl from './TuningControl';
import { setShowLogScale } from '../store/settingsSlice';
import { MIN_FREQ } from '../constants';

const SCALES = [
    { label: 'Log', log: true },
    { label: 'Linear', log: false },
];

// 50, 4000 → '50 Hz – 4 kHz'
const formatFrequency = hz => hz >= 1000 ? `${Number((hz / 1000).toFixed(1))} kHz` : `${Math.round(hz)} Hz`;
const formatRange = (minHz, maxHz) => `${formatFrequency(minHz)} – ${formatFrequency(maxHz)}`;

const SpectrumPanel = () => {
    const dispatch = useDispatch();
    const showLogScale = useSelector(state => state.settings.showLogScale);
    const spectrumRate = useSelector(state => state.analysis.spectrumSR);

    return (
        <Panel>
            <PanelHeader title="Spectrum">
                <Segmented role="group" aria-label="Frequency scale">
                    {SCALES.map(({ label, log }) => (
                        <Chip
                            key={label}
                            type="button"
                            selected={showLogScale === log}
                            aria-pressed={showLogScale === log}
                            onClick={() => dispatch(setShowLogScale(log))}
                        >
                            {label}
                        </Chip>
                    ))}
                </Segmented>
                {spectrumRate > 0 && <Range>{formatRange(MIN_FREQ, spectrumRate / 2)}</Range>}
                <Spacer />
                <TuningControl />
            </PanelHeader>
            <Body>
                <Plot>
                    <SpectrumCanvas />
                    <ChordBadge />
                </Plot>
            </Body>
        </Panel>
    );
};
export default SpectrumPanel;

const ChordBadge = () => {
    const chord = useSelector(state => state.analysis.predictedChords?.[0]?.name);
    if (!chord)
        return null;
    return (
        <Badge>
            <Caption>Chord</Caption>
            <ChordName>{chord}</ChordName>
        </Badge>
    );
};

const Segmented = Styled.div`
    display: flex;
    gap: 2px;
    padding: 2px;
    border-radius: 6px;
    background: var(--bg-surface);
`;

const Range = Styled.span`
    white-space: nowrap;
    font: 400 10px/1 var(--font-mono);
    color: var(--text-muted);
`;

const Body = Styled.div`
    flex: 1;
    display: flex;
    min-height: 0;
    padding: 0 12px 12px;
`;

const Plot = Styled.div`
    position: relative;
    flex: 1;
    min-width: 0;
    min-height: 0;
    overflow: hidden;
    border-radius: 6px;
    background: var(--bg-inset);
`;

const Badge = Styled.div`
    position: absolute;
    top: 14px;
    right: 14px;
    display: flex;
    flex-direction: column;
    padding: 8px 14px 10px 12px;
    border: 1px solid var(--border-strong);
    border-radius: 8px;
    background: color-mix(in srgb, var(--bg-panel) 85%, transparent);
    pointer-events: none;
`;

const ChordName = Styled.span`
    font: 600 30px/1.15 var(--font-ui);
    color: var(--accent-secondary-text);
`;
