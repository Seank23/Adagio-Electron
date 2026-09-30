import { useEffect, useRef } from 'react';
import { useStore } from 'react-redux';

// Follows a value in the Redux store without re-rendering the component.
export const useStoreListener = (selector, onChange) => {
    const store = useStore();
    const onChangeRef = useRef(onChange);

    useEffect(() => {
        onChangeRef.current = onChange;
    });

    useEffect(() => {
        let current = selector(store.getState());
        onChangeRef.current(current);

        return store.subscribe(() => {
            const next = selector(store.getState());
            if (next === current) return;
            current = next;
            onChangeRef.current(next);
        });
    }, [store, selector]);
};
