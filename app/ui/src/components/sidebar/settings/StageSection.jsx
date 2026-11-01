import { memo } from 'react';
import { useDispatch, useSelector } from 'react-redux';
import Styled from '@emotion/styled';
import { ChevronDown, ChevronRight } from 'lucide-react';
import SettingRow from './SettingRow';
import { toggleStage } from '../../../store/settingsSlice';
import { formatStageName, stageHasChanges } from '../../../utils/settings';

const StageSection = memo(({ stage, index, disabled }) => {
    const dispatch = useDispatch();
    const open = useSelector(state => state.settings.openStages.includes(stage.name));
    const changed = stageHasChanges(stage);
    const headerId = `stage-header-${stage.name}`;
    const bodyId = `stage-body-${stage.name}`;

    return (
        <Section>
            <Header
                type="button"
                id={headerId}
                aria-expanded={open}
                aria-controls={open ? bodyId : undefined}
                onClick={() => dispatch(toggleStage(stage.name))}
            >
                <Chevron aria-hidden>{open ? <ChevronDown size={16} /> : <ChevronRight size={16} />}</Chevron>
                <Index aria-hidden>{String(index + 1).padStart(2, '0')}</Index>
                <Name open={open}>{formatStageName(stage.name)}</Name>
                {changed && <ChangedDot aria-hidden title="Changed from its defaults" />}
            </Header>
            {open && (
                <Body role="region" id={bodyId} aria-labelledby={headerId}>
                    {stage.settings.map(setting => (
                        <SettingRow key={setting.key} stage={stage.name} setting={setting} disabled={disabled} />
                    ))}
                </Body>
            )}
        </Section>
    );
});
StageSection.displayName = 'StageSection';
export default StageSection;

const Section = Styled.section`
    border-bottom: 1px solid var(--border-subtle);
`;

const Header = Styled.button`
    display: flex;
    align-items: center;
    gap: 8px;
    width: 100%;
    height: 36px;
    padding: 0 16px 0 12px;
    border: none;
    background: none;
    text-align: left;
    cursor: pointer;
    &:hover { background: var(--bg-surface); }
    &:focus-visible { outline: 2px solid var(--accent-primary); outline-offset: -2px; }
`;

const Chevron = Styled.span`
    display: inline-flex;
    flex-shrink: 0;
    color: var(--text-secondary);
`;

const Index = Styled.span`
    flex-shrink: 0;
    font: 400 10px/1 var(--font-mono);
    color: var(--text-muted);
`;

const Name = Styled('span', { shouldForwardProp: prop => prop !== 'open' })`
    flex: 1;
    min-width: 0;
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
    font: ${({ open }) => open ? 600 : 500} 12px/1 var(--font-ui);
    color: var(--text-primary);
`;

const ChangedDot = Styled.span`
    flex-shrink: 0;
    width: 6px;
    height: 6px;
    border-radius: 50%;
    background: var(--accent-primary-fg);
`;

const Body = Styled.div`
    display: flex;
    flex-direction: column;
    gap: 14px;
    padding: 4px 16px 16px;
`;
