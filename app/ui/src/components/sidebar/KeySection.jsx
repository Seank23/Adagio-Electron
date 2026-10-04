import { useMemo, useRef } from 'react';
import { useSelector } from 'react-redux';
import Styled from '@emotion/styled';
import Caption from '../controls/Caption';
import { Bar, Percent, SectionTitle } from './BarRow';
import { barWidths, formatPercent } from '../../utils/bars';
import { NOTE_NAMES } from '../../utils/music';
import { selectPitchClassPercents } from '../../store/analysisSlice';
import { useStoreFrameListener } from '../../hooks/useStoreFrameListener';

const ROLE_COLORS = {
    tonic: { note: 'var(--accent-secondary-text)', bar: 'var(--accent-secondary-fg)', percent: 'var(--text-secondary)' },
    inKey: { note: 'var(--text-primary)', bar: 'var(--accent-primary-fg)', percent: 'var(--text-secondary)' },
    outOfKey: { note: 'var(--text-muted)', bar: 'var(--border-strong)', percent: 'var(--text-muted)' },
};

const roleOf = (key, scale, pitchClass) => {
    if (!key) return 'inKey';
    if (pitchClass === key.tonicClass) return 'tonic';
    return scale.has(pitchClass) ? 'inKey' : 'outOfKey';
};

const KeySection = () => {
    const key = useSelector(state => state.analysis.detectedKey);
    const scale = useMemo(() => new Set(key?.scaleClasses), [key]);
    const barRefs = useRef([]);
    const percentRefs = useRef([]);

    useStoreFrameListener(selectPitchClassPercents, percents => {
        const widths = barWidths(percents);
        percents.forEach((percent, pitchClass) => {
            const bar = barRefs.current[pitchClass];
            const label = percentRefs.current[pitchClass];
            if (bar) bar.style.width = widths[pitchClass];
            if (label) label.textContent = formatPercent(percent);
        });
    });

    return (
        <Section>
            <SectionTitle>Key</SectionTitle>
            <KeyName>{key?.name ?? '—'}</KeyName>
            <PitchCaption>Pitch classes</PitchCaption>
            <Rows>
                {NOTE_NAMES.map((name, pitchClass) => {
                    const colors = ROLE_COLORS[roleOf(key, scale, pitchClass)];
                    return (
                        <Row key={name}>
                            <Note style={{ color: colors.note }}>{name}</Note>
                            <Bar
                                height={6}
                                color={colors.bar}
                                fillRef={element => { barRefs.current[pitchClass] = element; }}
                            />
                            <Percent
                                style={{ width: 44 }}
                                color={colors.percent}
                                ref={element => { percentRefs.current[pitchClass] = element; }}
                            />
                        </Row>
                    );
                })}
            </Rows>
        </Section>
    );
};
export default KeySection;

const Section = Styled.section`
    display: flex;
    flex-direction: column;
    gap: 6px;
    padding: 14px 16px 16px;
`;

const KeyName = Styled.span`
    font: 600 26px/1.15 var(--font-ui);
    color: var(--text-primary);
    white-space: nowrap;
`;

const PitchCaption = Styled(Caption)`
    margin-top: 2px;
`;

const Rows = Styled.div`
    display: flex;
    flex-direction: column;
    gap: 5px;
    margin-top: 2px;
`;

const Row = Styled.div`
    display: flex;
    align-items: center;
    gap: 8px;
    height: 18px;
`;

const Note = Styled.span`
    flex-shrink: 0;
    width: 22px;
    font: 500 11px/1 var(--font-mono);
`;
