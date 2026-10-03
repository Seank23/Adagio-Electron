import { useEffect } from 'react';
import Styled from '@emotion/styled';
import { useDispatch, useSelector } from 'react-redux';
import { Gauge, Locate, Repeat, Volume2 } from 'lucide-react';
import { Panel } from './Panel';
import IconButton from './controls/IconButton';
import Chip from './controls/Chip';
import Caption from './controls/Caption';
import Divider from './controls/Divider';
import Readout from './controls/Readout';
import EngineSlider from './controls/EngineSlider';
import { selectIsFileOpen } from '../store/playbackSlice';
import { toggleFollow, toggleRepeat } from '../store/settingsSlice';
import { LIMITS } from '../utils/protocol';
import { useEngineCommands } from '../hooks/useEngineCommands';
import { useEngineSlider } from '../hooks/useEngineSlider';
import { useReport } from '../hooks/useReport';
import { formatClock } from '../utils/format';

const INITIAL_VOLUME = 0.2;
const SPEED_PRESETS = [50, 75, 100];
const HINTS = ['Space  play / pause', '←/→  seek 5 s', '.  step'];
const EMPTY_HINTS = ['Ctrl O  open a file'];

const toPercent = value => Math.round(value * 100);

const TransportBar = () => {
    const dispatch = useDispatch();
    const commands = useEngineCommands();
    const report = useReport();

    const isFileOpen = useSelector(selectIsFileOpen);
    const engineSpeed = useSelector(state => state.playback.speed);
    const engineVolume = useSelector(state => state.playback.volume);
    const duration = useSelector(state => state.playback.duration);
    const connectionState = useSelector(state => state.app.connectionState);
    const repeat = useSelector(state => state.settings.repeat);
    const follow = useSelector(state => state.settings.follow);

    const speed = useEngineSlider(engineSpeed, commands.setSpeed, false);
    const volume = useEngineSlider(engineVolume, commands.setVolume);

    useEffect(() => {
        if (connectionState !== 'connected')
            return;
        commands.setVolume(INITIAL_VOLUME).then(report);
    }, [connectionState, commands, report]);

    return (
        <Bar>
            <TimeDisplay dimmed={!isFileOpen}>
                {isFileOpen
                    ? <PositionReadout />
                    : <Readout caption="Position">{formatClock(0)}</Readout>}
                <Divider height={26} />
                <Readout caption="Length" muted>{isFileOpen ? formatClock(duration) : '--:--.---'}</Readout>
            </TimeDisplay>
            <Divider />
            <Group inert={!isFileOpen}>
                <Caption>Speed</Caption>
                <EngineSlider
                    icon={<Gauge size={16} />}
                    label="Speed"
                    min={toPercent(LIMITS.speedMin)}
                    max={toPercent(LIMITS.speedMax)}
                    {...speed}
                />
                <Presets>
                    {SPEED_PRESETS.map(preset => (
                        <Chip
                            key={preset}
                            type="button"
                            selected={speed.percent === preset}
                            onClick={async () => report(await commands.setSpeed(preset / 100))}
                        >
                            {preset}
                        </Chip>
                    ))}
                </Presets>
            </Group>
            <Divider />
            <Group>
                <Caption>Volume</Caption>
                <EngineSlider
                    icon={<Volume2 size={16} />}
                    label="Volume"
                    min={toPercent(LIMITS.volumeMin)}
                    max={toPercent(LIMITS.volumeMax)}
                    {...volume}
                />
            </Group>
            <Divider />
            <Modes inert={!isFileOpen}>
                <IconButton
                    variant={repeat ? 'active' : 'ghost'}
                    label="Repeat"
                    icon={<Repeat size={16} />}
                    onClick={() => dispatch(toggleRepeat())}
                />
                <IconButton
                    variant={follow ? 'active' : 'ghost'}
                    label="Follow the playhead"
                    icon={<Locate size={16} />}
                    onClick={() => dispatch(toggleFollow())}
                />
                <ModeLabel>Follow</ModeLabel>
            </Modes>
            <Hints>
                <HintLine />
                {(isFileOpen ? HINTS : EMPTY_HINTS).map(hint => <Hint key={hint}>{hint}</Hint>)}
            </Hints>
        </Bar>
    );
};
export default TransportBar;

const PositionReadout = () => {
    const currentTime = useSelector(state => state.playback.currentTime);
    return <Readout caption="Position">{formatClock(currentTime)}</Readout>;
};

const Bar = Styled(Panel)`
    flex-direction: row;
    align-items: center;
    gap: 18px;
    padding: 0 16px;
`;

const TimeDisplay = Styled('div', { shouldForwardProp: prop => prop !== 'dimmed' })`
    display: flex;
    align-items: center;
    flex-shrink: 0;
    gap: 10px;
    padding: 7px 14px 7px 12px;
    border: 1px solid var(--border-subtle);
    border-radius: 6px;
    background: var(--bg-inset);
    opacity: ${({ dimmed }) => dimmed ? .5 : 1};
`;

const dimWhenInert = `
    &[inert] { opacity: .35; }
`;

const Group = Styled.div`
    display: flex;
    align-items: center;
    flex-shrink: 0;
    gap: 10px;
    ${dimWhenInert}
`;

const Presets = Styled.div`
    display: flex;
    gap: 4px;
`;

const Modes = Styled.div`
    display: flex;
    align-items: center;
    flex-shrink: 0;
    gap: 6px;
    ${dimWhenInert}
`;

const ModeLabel = Styled.span`
    font: 500 11px/1 var(--font-ui);
    color: var(--text-secondary);
`;

// The hints take what room is left and give way first. They wrap rather than shrink,
// and only the first line shows, so a hint that doesn't fit disappears whole instead
// of being cut off.
const Hints = Styled.div`
    flex: 1 1 0;
    min-width: 0;
    display: flex;
    flex-wrap: wrap;
    justify-content: flex-end;
    column-gap: 24px;
    height: 12px;
    overflow: hidden;
`;

const HintLine = Styled.span`
    width: 0;
    height: 12px;
`;

const Hint = Styled.span`
    white-space: pre;
    font: 400 10px/12px var(--font-mono);
    color: var(--text-muted);
`;
