/* eslint-disable react-hooks/exhaustive-deps */
import { createContext, useMemo } from "react";
import _ from 'lodash';

export const AudioContext = createContext(null);

export function useAudioProvider() {

    const returnObj = {
    };

    const providerValue = useMemo(
        () => returnObj, [
        ]
    );

    return [providerValue, returnObj];
}