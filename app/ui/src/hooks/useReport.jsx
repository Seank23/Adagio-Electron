import { useCallback } from 'react';
import { useDispatch } from 'react-redux';
import { setStatusMessage } from '../store/appSlice';

// Every command's result goes through here, so a refusal always reaches the status bar.
// It hands the result back so a caller can still read it.
export const useReport = () => {
    const dispatch = useDispatch();
    return useCallback(result => {
        if (result?.ok === false)
            dispatch(setStatusMessage({ type: 'error', message: result.error }));
        return result;
    }, [dispatch]);
};
