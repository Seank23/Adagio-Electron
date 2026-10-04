import Styled from '@emotion/styled';
import { Panel, PanelHeader, Spacer } from './Panel';
import Caption from './controls/Caption';
import ChordKeyboard from './ChordKeyboard';

const NoteActivityPanel = () => (
    <Panel>
        <PanelHeader title="Note activity">
            <Hint>rolling score by note</Hint>
            <Spacer />
            <LegendLabel>less</LegendLabel>
            <Ramp />
            <LegendLabel>more</LegendLabel>
        </PanelHeader>
        <Body>
            <WindowCaption>Piano heatmap</WindowCaption>
            <KeyboardBox>
                <ChordKeyboard />
            </KeyboardBox>
        </Body>
    </Panel>
);
export default NoteActivityPanel;

const Hint = Styled.span`
    overflow: hidden;
    white-space: nowrap;
    text-overflow: ellipsis;
    font: 400 10px/1 var(--font-ui);
    color: var(--text-muted);
`;

const LegendLabel = Styled.span`
    font: 400 9px/1 var(--font-ui);
    color: var(--text-muted);
`;

const Ramp = Styled.span`
    flex-shrink: 0;
    width: 64px;
    height: 6px;
    margin: 0 -4px;
    border-radius: 3px;
    background: linear-gradient(90deg, var(--bg-inset), var(--accent-primary-fg));
`;

const Body = Styled.div`
    flex: 1;
    display: flex;
    flex-direction: column;
    min-height: 0;
    padding: 0 12px 12px;
`;

const WindowCaption = Styled(Caption)`
    display: flex;
    align-items: center;
    flex-shrink: 0;
    height: 15px;
`;

const KeyboardBox = Styled.div`
    position: relative;
    flex: 1;
    min-height: 0;
`;
