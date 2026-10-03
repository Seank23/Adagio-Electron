import Styled from '@emotion/styled';
import { keyframes } from '@emotion/react';
import { useSelector } from 'react-redux';
import { FolderOpen, Loader2, Upload } from 'lucide-react';
import { useOpenFile } from '../hooks/useOpenFile';
import { fileNameOf } from '../utils/format';

const EmptyState = ({ isDragging }) => {
    const openFile = useOpenFile();
    const loadingFile = useSelector(state => state.app.loadingFile);

    return (
        <Area>
            <Card dragging={isDragging}>
                <IconCircle dragging={isDragging}>
                    <Upload size={26} />
                </IconCircle>
                {loadingFile
                    ? (
                        <Loading role="status">
                            <Spinner><Loader2 size={18} /></Spinner>
                            <LoadingName title={loadingFile}>{fileNameOf(loadingFile)}</LoadingName>
                        </Loading>
                    )
                    : <Title>Drop an audio file to analyse</Title>}
                <Subline>
                    Adagio follows the notes, chords and key as the track plays, and can slow it
                    to 20% without changing its pitch.
                </Subline>
                <OpenButton type="button" onClick={openFile} disabled={loadingFile !== ''}>
                    <FolderOpen size={16} />
                    <span>Open file…</span>
                    <Shortcut>Ctrl O</Shortcut>
                </OpenButton>
                <Formats>WAV · MP3 · FLAC</Formats>
            </Card>
        </Area>
    );
};
export default EmptyState;

const Area = Styled.div`
    display: flex;
    align-items: center;
    justify-content: center;
    min-height: 0;
    overflow: hidden;
`;

const Card = Styled('div', { shouldForwardProp: prop => prop !== 'dragging' })`
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 14px;
    padding: 48px 64px 40px;
    border: 1.5px dashed ${({ dragging }) => dragging ? 'var(--accent-primary-fg)' : 'var(--border-strong)'};
    border-radius: 14px;
    background: var(--bg-panel);
    transition: border-color .12s;
`;

const IconCircle = Styled('div', { shouldForwardProp: prop => prop !== 'dragging' })`
    display: flex;
    align-items: center;
    justify-content: center;
    width: 64px;
    height: 64px;
    border-radius: 50%;
    background: ${({ dragging }) => dragging
        ? 'color-mix(in srgb, var(--accent-primary) 45%, transparent)'
        : 'var(--accent-soft)'};
    color: var(--accent-primary-fg);
    transition: background .12s;
`;

const Title = Styled.div`
    font: 600 18px/1.3 var(--font-ui);
    color: var(--text-primary);
`;

const Loading = Styled.div`
    display: flex;
    align-items: center;
    gap: 10px;
    max-width: 400px;
    font: 600 18px/1.3 var(--font-ui);
    color: var(--text-primary);
`;

const rotate = keyframes`
    to { transform: rotate(360deg); }
`;

const Spinner = Styled.span`
    display: inline-flex;
    flex-shrink: 0;
    color: var(--accent-primary-fg);
    animation: ${rotate} 1s linear infinite;
`;

const LoadingName = Styled.span`
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
`;

const Subline = Styled.p`
    width: 400px;
    margin: 0 0 20px;
    text-align: center;
    font: 400 12px/1.5 var(--font-ui);
    color: var(--text-secondary);
`;

const OpenButton = Styled.button`
    display: inline-flex;
    align-items: center;
    gap: 8px;
    padding: 10px 16px;
    border: none;
    border-radius: 8px;
    background: var(--accent-primary);
    color: var(--text-on-accent);
    font: 600 13px/1 var(--font-ui);
    cursor: pointer;
    &:hover:not(:disabled) { filter: brightness(1.1); }
    &:disabled {
        opacity: .5;
        cursor: default;
    }
    &:focus-visible {
        outline: 2px solid var(--accent-primary);
        outline-offset: 2px;
    }
`;

const Shortcut = Styled.span`
    font: 400 10px/1 var(--font-mono);
    opacity: .6;
`;

const Formats = Styled.span`
    font: 400 10px/1 var(--font-mono);
    letter-spacing: 1px;
    color: var(--text-muted);
`;
