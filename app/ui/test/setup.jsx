import { afterEach } from 'vitest';
import { cleanup } from '@testing-library/react';
import { clearFrames } from '../src/engine-client/FrameStore';

// devPerf decides at import whether to instrument; tests don't want its console tables.
try {
    localStorage.setItem('adagio:perf', 'off');
} catch {
    // no storage: measure() still calls straight through
}

// jsdom has no layout engine, so these browser APIs are missing. antd and the theme
// provider read matchMedia at import, which is why this runs in setup, before them.
if (!window.matchMedia) {
    window.matchMedia = query => ({
        matches: false,
        media: query,
        onchange: null,
        addEventListener: () => {},
        removeEventListener: () => {},
        addListener: () => {},
        removeListener: () => {},
        dispatchEvent: () => false,
    });
}

if (!globalThis.ResizeObserver) {
    globalThis.ResizeObserver = class {
        observe() {}
        unobserve() {}
        disconnect() {}
    };
}

// jsdom implements no 2D context and logs an error for each request; the canvases
// already treat a missing context as nothing to draw.
HTMLCanvasElement.prototype.getContext = () => null;

afterEach(() => {
    cleanup();
    clearFrames();
    delete window.api;
});
