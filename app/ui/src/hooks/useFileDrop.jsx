import { useEffect, useState } from 'react';
import { useDispatch } from 'react-redux';
import { setStatusMessage } from '../store/appSlice';
import { fileNameOf, formatOf } from '../utils/format';

const SUPPORTED_FORMATS = ['WAV', 'MP3', 'FLAC'];

const carriesFiles = event => event.dataTransfer?.types?.includes('Files') ?? false;

// Window-level drag and drop. Every dragover and drop is cancelled, whatever it carries.
export const useFileDrop = ({ onFile }) => {
    const dispatch = useDispatch();
    const [isDragging, setIsDragging] = useState(false);

    useEffect(() => {
        const onDragOver = event => {
            event.preventDefault();
            const files = carriesFiles(event);
            event.dataTransfer.dropEffect = files ? 'copy' : 'none';
            setIsDragging(files);
        };

        const onDragLeave = event => {
            if (event.relatedTarget === null)
                setIsDragging(false);
        };

        const onDrop = event => {
            event.preventDefault();
            setIsDragging(false);

            const file = event.dataTransfer?.files?.[0];
            if (!file)
                return;
            if (!window.api?.getPathForFile) {
                dispatch(setStatusMessage({ type: 'error', message: 'Opening a file needs the desktop app.' }));
                return;
            }

            const path = window.api.getPathForFile(file);
            if (!path || !SUPPORTED_FORMATS.includes(formatOf(fileNameOf(path)))) {
                dispatch(setStatusMessage({ type: 'error', message: 'Adagio opens WAV, MP3 and FLAC files.' }));
                return;
            }
            onFile(path);
        };

        window.addEventListener('dragenter', onDragOver);
        window.addEventListener('dragover', onDragOver);
        window.addEventListener('dragleave', onDragLeave);
        window.addEventListener('drop', onDrop);
        return () => {
            window.removeEventListener('dragenter', onDragOver);
            window.removeEventListener('dragover', onDragOver);
            window.removeEventListener('dragleave', onDragLeave);
            window.removeEventListener('drop', onDrop);
        };
    }, [dispatch, onFile]);

    return isDragging;
};
