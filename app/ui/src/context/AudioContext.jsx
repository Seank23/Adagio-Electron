/* eslint-disable react-hooks/exhaustive-deps */
import { createContext, useMemo, useState } from "react";
import _ from 'lodash';

export const AudioContext = createContext(null);

export function useAudioProvider() {
    const [status, setStatus] = useState({});
    const [fileOpen, setFileOpen] = useState(false);

    const returnObj = {
        status,
        setStatus,
        fileOpen,
        setFileOpen,
    };

    const providerValue = useMemo(
        () => returnObj, [
            status,
            setStatus,
            fileOpen,
            setFileOpen,
        ]
    );

    return [providerValue, returnObj];
}