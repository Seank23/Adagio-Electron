import { createContext, useMemo, useState, useCallback } from "react";
import { useEngineEvents } from "../hooks/useEngineEvents";
import { EVENT_TYPE } from "../utils/utils";

export const AudioContext = createContext(null);

export function useAudioProvider() {
    const [status, setStatus] = useState({});
    const [fileOpen, setFileOpen] = useState(false);
    const [audioPlaying, setAudioPlaying] = useState(false);

    const eventCallback = useCallback(msg => {
        switch (msg.type) {
          case EVENT_TYPE.END_OF_PLAY:
            console.log('Track finished');
        }
      }, []);
    useEngineEvents(eventCallback);

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