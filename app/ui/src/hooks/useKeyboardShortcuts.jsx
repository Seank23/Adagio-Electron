import { useEffect } from 'react';
import { useStore } from 'react-redux';
import { selectIsFileOpen, selectIsPlaying } from '../store/playbackSlice';
import { setPreferencesOpen } from '../store/appSlice';
import { useEngineCommands } from './useEngineCommands';
import { useReport } from './useReport';
import { useOpenFile } from './useOpenFile';
import { useFrameStep } from './useStepRepeat';
import { clamp } from '../utils/math';

const SEEK_STEP_SECONDS = 5;

// Typing, and a focused slider's own arrow keys, take precedence over the shortcuts.
const ownsKeys = target =>
    target instanceof Element && (
        target.isContentEditable
        || ['INPUT', 'TEXTAREA', 'SELECT'].includes(target.tagName)
        || target.closest('[role="slider"]') !== null
    );

export const useKeyboardShortcuts = () => {
    const store = useStore();
    const commands = useEngineCommands();
    const report = useReport();
    const openFile = useOpenFile();
    const step = useFrameStep();

    useEffect(() => {
        let stepFailed = false;

        const preferencesOpen = () => store.getState().app.preferencesOpen;

        const onKeyDown = event => {
            if (preferencesOpen() || ownsKeys(event.target))
                return;

            const command = event.ctrlKey || event.metaKey;
            if (command && !event.altKey && !event.shiftKey && event.key === ',') {
                event.preventDefault();
                store.dispatch(setPreferencesOpen(true));
                return;
            }
            if (command && !event.altKey && !event.shiftKey && event.key.toLowerCase() === 'o') {
                event.preventDefault();
                if (!event.repeat)
                    openFile();
                return;
            }
            if (command || event.altKey)
                return;

            const state = store.getState();
            if (!selectIsFileOpen(state))
                return;

            switch (event.key) {
            case ' ':
                event.preventDefault();
                if (!event.repeat)
                    (selectIsPlaying(state) ? commands.pause() : commands.play()).then(report);
                break;
            case 'ArrowLeft':
            case 'ArrowRight': {
                event.preventDefault();
                const { currentTime, duration } = state.playback;
                const step = event.key === 'ArrowLeft' ? -SEEK_STEP_SECONDS : SEEK_STEP_SECONDS;
                commands.seek(clamp(currentTime + step, 0, duration)).then(report);
                break;
            }
            case '.':
                event.preventDefault();
                if (!event.repeat)
                    stepFailed = false;
                else if (stepFailed)
                    break;
                step().then(result => {
                    if (result && !result.ok)
                        stepFailed = true;
                });
                break;
            default:
                break;
            }
        };

        // A button activates on Space's keyup, so that is cancelled too, except in the modal,
        // where Space has to press the focused chip.
        const onKeyUp = event => {
            if (event.key === ' ' && !preferencesOpen() && !ownsKeys(event.target))
                event.preventDefault();
        };

        window.addEventListener('keydown', onKeyDown);
        window.addEventListener('keyup', onKeyUp);
        return () => {
            window.removeEventListener('keydown', onKeyDown);
            window.removeEventListener('keyup', onKeyUp);
        };
    }, [store, commands, report, openFile, step]);
};
