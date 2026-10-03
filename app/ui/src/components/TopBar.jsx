import Styled from '@emotion/styled';
import { useSelector } from 'react-redux';
import { Eraser, FolderOpen, Music, Pause, Play, SkipBack, Square, StepForward, X } from 'lucide-react';
import { Panel } from './Panel';
import IconButton from './controls/IconButton';
import Divider from './controls/Divider';
import { selectIsFileOpen, selectIsPlaying } from '../store/playbackSlice';
import { useEngineCommands } from '../hooks/useEngineCommands';
import { useReport } from '../hooks/useReport';
import { useOpenFile } from '../hooks/useOpenFile';
import { selectCanStep, useStepRepeat } from '../hooks/useStepRepeat';
import { fileNameOf, formatChannels, formatDuration, formatOf, formatSampleRate } from '../utils/format';

// Transport glyphs are solid in the design.
const solid = { fill: 'currentColor' };

const TopBar = () => {
    const commands = useEngineCommands();
    const report = useReport();
    const openFile = useOpenFile();
    const isFileOpen = useSelector(selectIsFileOpen);
    const isPlaying = useSelector(selectIsPlaying);
    const isLoading = useSelector(state => state.app.loadingFile !== '');
    const canStep = useSelector(selectCanStep);
    const stepHandlers = useStepRepeat();

    const onPlayPause = async () => report(await (isPlaying ? commands.pause() : commands.play()));

    return (
        <Bar>
            <Left>
                <Brand>
                    <Logo>A</Logo>
                    <BrandName>Adagio</BrandName>
                </Brand>
                <Divider height={20} />
                <TrackInfo />
            </Left>
            <Centre>
                <IconButton
                    label="Back to start"
                    icon={<SkipBack size={16} {...solid} />}
                    disabled={!isFileOpen}
                    onClick={async () => report(await commands.seek(0))}
                />
                <IconButton
                    variant="primary"
                    label={isPlaying ? 'Pause' : 'Play'}
                    icon={isPlaying ? <Pause size={18} {...solid} /> : <Play size={18} {...solid} />}
                    disabled={!isFileOpen}
                    onClick={onPlayPause}
                />
                <IconButton
                    label="Step one analysis frame (.)"
                    icon={<StepForward size={16} {...solid} />}
                    disabled={!canStep}
                    {...stepHandlers}
                />
                <IconButton
                    label="Stop"
                    icon={<Square size={16} {...solid} />}
                    disabled={!isFileOpen}
                    onClick={async () => report(await commands.stop())}
                />
                <Divider height={20} />
                <IconButton
                    label="Reset analysis: clears the spectrum, key and chords"
                    icon={<Eraser size={16} />}
                    disabled={!isFileOpen}
                    onClick={async () => report(await commands.resetAnalysis())}
                />
            </Centre>
            <Right>
                <OpenButton type="button" onClick={openFile} disabled={isLoading}>
                    <FolderOpen size={16} />
                    <span>Open…</span>
                    <Shortcut>Ctrl O</Shortcut>
                </OpenButton>
                {isFileOpen && (
                    <IconButton
                        label="Close file"
                        icon={<X size={16} />}
                        onClick={async () => report(await commands.clear())}
                    />
                )}
            </Right>
        </Bar>
    );
};
export default TopBar;

const TrackInfo = () => {
    const path = useSelector(state => state.playback.track?.path);
    const sampleRate = useSelector(state => state.playback.track?.sampleRate);
    const channels = useSelector(state => state.playback.track?.channels);
    const duration = useSelector(state => state.playback.track?.duration);

    if (!path)
        return <NoFile>No file open</NoFile>;

    const name = fileNameOf(path);
    const meta = [formatOf(name), formatSampleRate(sampleRate), formatChannels(channels), formatDuration(duration)]
        .filter(Boolean)
        .join(' · ');

    return (
        <Track title={path}>
            <TrackIcon><Music size={16} /></TrackIcon>
            <TrackName>{name}</TrackName>
            <Meta>{meta}</Meta>
        </Track>
    );
};

const Bar = Styled(Panel)`
    display: grid;
    grid-template-columns: minmax(0, 1fr) auto minmax(0, 1fr);
    align-items: center;
    gap: 12px;
    padding: 0 10px 0 14px;
`;

const Left = Styled.div`
    display: flex;
    align-items: center;
    gap: 12px;
    min-width: 0;
`;

const Brand = Styled.div`
    display: flex;
    align-items: center;
    gap: 8px;
    flex-shrink: 0;
`;

const Logo = Styled.span`
    display: inline-flex;
    align-items: center;
    justify-content: center;
    width: 20px;
    height: 20px;
    border-radius: 5px;
    background: var(--accent-primary);
    color: var(--text-on-accent);
    font: 700 12px/1 var(--font-ui);
`;

const BrandName = Styled.span`
    font: 600 13px/1 var(--font-ui);
    color: var(--text-primary);
`;

const Track = Styled.div`
    display: flex;
    align-items: center;
    gap: 8px;
    min-width: 0;
`;

const TrackIcon = Styled.span`
    display: inline-flex;
    flex-shrink: 0;
    color: var(--text-secondary);
`;

const TrackName = Styled.span`
    min-width: 0;
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
    font: 500 12px/1.2 var(--font-ui);
    color: var(--text-primary);
`;

const Meta = Styled.span`
    flex-shrink: 0;
    white-space: nowrap;
    font: 400 11px/1.2 var(--font-ui);
    color: var(--text-muted);
`;

const NoFile = Styled.span`
    font: 400 12px/1.2 var(--font-ui);
    color: var(--text-secondary);
`;

const Centre = Styled.div`
    display: flex;
    align-items: center;
    gap: 6px;
`;

const Right = Styled.div`
    display: flex;
    align-items: center;
    justify-content: flex-end;
    gap: 12px;
`;

const OpenButton = Styled.button`
    display: inline-flex;
    align-items: center;
    gap: 6px;
    padding: 6px 12px 6px 10px;
    border: none;
    border-radius: 6px;
    background: var(--bg-raised);
    color: var(--text-primary);
    font: 500 12px/1.2 var(--font-ui);
    cursor: pointer;
    &:hover:not(:disabled) {
        filter: brightness(1.15);
    }
    &:disabled {
        opacity: .35;
        cursor: default;
    }
    &:focus-visible {
        outline: 2px solid var(--accent-primary);
        outline-offset: 1px;
    }
`;

const Shortcut = Styled.span`
    font: 400 10px/1 var(--font-mono);
    color: var(--text-muted);
`;
