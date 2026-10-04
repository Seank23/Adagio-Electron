import { useLayoutEffect, useRef } from 'react';
import { useSelector } from 'react-redux';
import Styled from '@emotion/styled';
import { Bar, Percent, SectionTitle } from './BarRow';
import { barWidths, formatPercent } from '../../utils/bars';
import { selectChordNamesKey, selectChordProbabilities } from '../../store/analysisSlice';
import { useStoreFrameListener } from '../../hooks/useStoreFrameListener';

const ChordSection = () => {
    const namesKey = useSelector(selectChordNamesKey);
    const names = namesKey ? namesKey.split('|') : [];
    const barRefs = useRef([]);
    const percentRefs = useRef([]);

    const paint = probabilities => {
        if (!probabilities) return;
        const widths = barWidths(probabilities);
        probabilities.forEach((probability, index) => {
            const probPercent = probability * 100;
            const bar = barRefs.current[index];
            const label = percentRefs.current[index];
            if (bar) bar.style.width = widths[index];
            if (label) label.textContent = formatPercent(probPercent);
        });
    };
    const latest = useStoreFrameListener(selectChordProbabilities, paint);

    // Rows that have just appeared start empty. Fill them now rather than on the next
    // event, which never comes while paused.
    useLayoutEffect(() => {
        paint(latest.current);
    }, [namesKey]); // eslint-disable-line react-hooks/exhaustive-deps

    return (
        <Section>
            <ChordTitle>Chord</ChordTitle>
            {names.map((name, index) => {
                const top = index === 0;
                return (
                    <Row key={`${index}-${name}`} top={top}>
                        <Name top={top}>{name}</Name>
                        <Bar
                            height={4}
                            color={top ? 'var(--accent-secondary-fg)' : 'var(--text-muted)'}
                            fillRef={element => { barRefs.current[index] = element; }}
                        />
                        <Percent
                            style={{ width: 40 }}
                            color="var(--text-secondary)"
                            ref={element => { percentRefs.current[index] = element; }}
                        />
                    </Row>
                );
            })}
        </Section>
    );
};
export default ChordSection;

const withoutTop = { shouldForwardProp: prop => prop !== 'top' };

const Section = Styled.section`
    display: flex;
    flex-direction: column;
    gap: 4px;
    padding: 14px 16px 16px;
    border-top: 1px solid var(--border-subtle);
`;

const ChordTitle = Styled(SectionTitle)`
    margin-bottom: 4px;
`;

const Row = Styled('div', withoutTop)`
    display: flex;
    align-items: center;
    gap: 10px;
    height: 26px;
    padding: 0 8px;
    border-radius: 5px;
    background: ${({ top }) => top ? 'var(--accent-secondary-soft)' : 'none'};
`;

const Name = Styled('span', withoutTop)`
    flex-shrink: 0;
    width: 88px;
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
    font: ${({ top }) => top ? 600 : 500} 12px/1 var(--font-ui);
    color: ${({ top }) => top ? 'var(--accent-secondary-text)' : 'var(--text-primary)'};
`;
