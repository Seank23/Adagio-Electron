import { createContext, useMemo, useState } from "react";

export const AudioContext = createContext(null);

export function useAudioProvider() {
    const [status, setStatus] = useState({});
    const [fileOpen, setFileOpen] = useState(false);
    const [audioPlaying, setAudioPlaying] = useState(false);

    const returnObj = {
        status,
        setStatus,
        fileOpen,
        setFileOpen,
        audioPlaying,
        setAudioPlaying
    };

    const providerValue = useMemo(
        () => returnObj, 
        [status, setStatus, fileOpen, setFileOpen, audioPlaying, setAudioPlaying]
    );

    return [providerValue, returnObj];
}