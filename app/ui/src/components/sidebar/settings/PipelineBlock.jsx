import { useState } from 'react';
import { useSelector } from 'react-redux';
import Styled from '@emotion/styled';
import { Check, RotateCcw } from 'lucide-react';
import Select from '../../controls/Select';
import { SectionTitle } from '../BarRow';
import { usePipelineCommands } from '../../../hooks/usePipelineCommands';
import { selectActivePipeline, selectHasChanges, selectPipelines } from '../../../store/pipelineSlice';

const PipelineBlock = ({ stageCount, disabled }) => {
    const { switchTo, restoreDefaults } = usePipelineCommands();
    const active = useSelector(selectActivePipeline);
    const pipelines = useSelector(selectPipelines);
    const hasChanges = useSelector(selectHasChanges);
    // A switch builds the pipeline and restarts the analysis thread, so the select waits for the answer.
    const [busy, setBusy] = useState(false);

    const run = async request => {
        setBusy(true);
        try {
            await request();
        } finally {
            setBusy(false);
        }
    };

    // Only value and label: antd copies an option's other fields onto the closed select's element as attributes.
    const options = pipelines.map(pipeline => ({ value: pipeline.id, label: pipeline.name }));
    const descriptions = Object.fromEntries(pipelines.map(pipeline => [pipeline.id, pipeline.description]));

    return (
        <Block>
            <TitleRow>
                <SectionTitle>Pipeline</SectionTitle>
                <ResetAll
                    type="button"
                    disabled={disabled || busy || !hasChanges}
                    title="Return every setting in this pipeline to its default"
                    onClick={() => run(restoreDefaults)}
                >
                    <RotateCcw size={12} aria-hidden />
                    Reset all
                </ResetAll>
            </TitleRow>
            <Select
                label="Analysis pipeline"
                fill
                value={active}
                options={options}
                disabled={disabled || busy}
                // The value in force, not the choice: a refused switch leaves the select where it was.
                onChange={id => id !== active && run(() => switchTo(id))}
                optionRender={option => (
                    <OptionRow>
                        <CheckSlot>{option.value === active && <Check size={16} aria-hidden />}</CheckSlot>
                        <OptionText>
                            <OptionName>{option.data.label}</OptionName>
                            {descriptions[option.value] && <OptionDescription>{descriptions[option.value]}</OptionDescription>}
                        </OptionText>
                    </OptionRow>
                )}
            />
            <Note>{stageCount ? `${stageCount} stages · changes apply immediately` : 'Waiting for the engine'}</Note>
        </Block>
    );
};
export default PipelineBlock;

const Block = Styled.section`
    display: flex;
    flex-direction: column;
    gap: 8px;
    padding: 14px 16px;
    border-bottom: 1px solid var(--border-subtle);
`;

const TitleRow = Styled.div`
    display: flex;
    align-items: center;
    justify-content: space-between;
`;

const ResetAll = Styled.button`
    display: inline-flex;
    align-items: center;
    gap: 4px;
    padding: 2px 0;
    border: none;
    background: none;
    font: 500 11px/1 var(--font-ui);
    color: var(--text-secondary);
    cursor: pointer;
    &:hover:not(:disabled) { color: var(--text-primary); }
    &:disabled { color: var(--text-muted); cursor: default; }
    &:focus-visible { outline: 2px solid var(--accent-primary); outline-offset: 2px; border-radius: 3px; }
`;

const OptionRow = Styled.div`
    display: flex;
    align-items: flex-start;
    gap: 8px;
`;

const CheckSlot = Styled.span`
    display: inline-flex;
    flex-shrink: 0;
    width: 16px;
    height: 16px;
`;

const OptionText = Styled.span`
    display: flex;
    flex-direction: column;
    gap: 2px;
    min-width: 0;
    white-space: normal;
`;

const OptionName = Styled.span`
    font: 500 12px/16px var(--font-ui);
`;

const OptionDescription = Styled.span`
    font: 400 11px/15px var(--font-ui);
    color: var(--text-muted);
`;

const Note = Styled.span`
    font: 400 11px/1.3 var(--font-ui);
    color: var(--text-muted);
`;
