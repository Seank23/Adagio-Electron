import { useCallback, useRef } from 'react';
import Styled from '@emotion/styled';
import { Maximize2, ZoomIn, ZoomOut } from 'lucide-react';
import { Panel, PanelHeader, Spacer } from './Panel';
import IconButton from './controls/IconButton';
import AudioTimeline from './AudioTimeline';
import TimelineOverview from './TimelineOverview';
import TimelineRuler from './TimelineRuler';
import { formatDuration } from '../utils/format';

const BUTTON_ZOOM = 1.5;

const formatRange = ({ start, end }) =>
    `${formatDuration(start)} – ${formatDuration(end)}`;

// The view changes on every frame of a drag, so the range text and the overview are
// updated through refs rather than state.
const TimelinePanel = () => {
    const apiRef = useRef(null);
    const viewRef = useRef(null);
    const rangeRef = useRef(null);
    const drawOverviewRef = useRef(null);
    const drawRulerRef = useRef(null);

    const onView = useCallback(view => {
        viewRef.current = view;
        if (rangeRef.current)
            rangeRef.current.textContent = formatRange(view);
        drawOverviewRef.current?.();
        drawRulerRef.current?.();
    }, []);

    return (
        <Panel>
            <PanelHeader title="Timeline" height={32}>
                <Range ref={rangeRef} />
                <Spacer />
                <Hint>Drag the overview ↕ to zoom · ↔ to pan</Hint>
                <Actions>
                    <IconButton label="Zoom in" icon={<ZoomIn size={16} />} onClick={() => apiRef.current?.zoomBy(BUTTON_ZOOM)} />
                    <IconButton label="Zoom out" icon={<ZoomOut size={16} />} onClick={() => apiRef.current?.zoomBy(1 / BUTTON_ZOOM)} />
                    <IconButton label="Fit the whole track" icon={<Maximize2 size={16} />} onClick={() => apiRef.current?.fit()} />
                </Actions>
            </PanelHeader>
            <Body>
                <TimelineOverview viewRef={viewRef} apiRef={apiRef} drawRef={drawOverviewRef} />
                <Zoomed>
                    <TimelineRuler viewRef={viewRef} drawRef={drawRulerRef} />
                    <AudioTimeline apiRef={apiRef} onView={onView} />
                </Zoomed>
            </Body>
        </Panel>
    );
};
export default TimelinePanel;

const Range = Styled.span`
    white-space: pre;
    font: 400 10px/1 var(--font-mono);
    font-variant-numeric: tabular-nums;
    color: var(--text-muted);
`;

const Hint = Styled.span`
    white-space: nowrap;
    overflow: hidden;
    text-overflow: ellipsis;
    min-width: 0;
    font: 400 10px/1 var(--font-ui);
    color: var(--text-muted);
`;

const Actions = Styled.div`
    display: flex;
    flex-shrink: 0;
    gap: 2px;
    margin-right: -6px;
`;

const Body = Styled.div`
    flex: 1;
    display: flex;
    flex-direction: column;
    gap: 6px;
    min-height: 0;
    padding: 6px 12px 8px;
`;

const Zoomed = Styled.div`
    flex: 1;
    display: flex;
    flex-direction: column;
    min-height: 0;
`;
