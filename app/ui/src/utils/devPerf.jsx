// Dev-only renderer instrumentation. In a production build import.meta.env.DEV is
// false, so measure() just calls through and nothing else runs.
//
// What it reports, every few seconds in the console:
//   - each instrumented section (spectrum draw, socket frames and events, React
//     commits): how often it ran, its mean and its worst time;
//   - per display frame, the time spent in those sections, and how many frames went
//     over the 4 ms budget.
// That is script time only. Style, layout and paint are not visible from JS, so the
// Performance panel is still the authority on a frame's full cost. Every section is
// also a performance.measure, so it shows under Timings in a recording.
//
// Frames of 50 ms or more are logged as they happen, with the scripts that ran in
// them, from the browser's long-animation-frame entries.
//
// Turn it off with localStorage.setItem('adagio:perf', 'off') and a reload.

const FRAME_BUDGET_MS = 4;
const REPORT_INTERVAL_MS = 5000;

const isEnabled = () => {
    try {
        return localStorage.getItem('adagio:perf') !== 'off';
    } catch {
        return true;
    }
};

// Written so the build folds it to false and drops everything behind it.
const ENABLED = import.meta.env.DEV && isEnabled();

const sections = new Map();
let frameWorkMs = 0;
let frames = [];

const record = (name, durationMs) => {
    let section = sections.get(name);
    if (!section) {
        section = { count: 0, totalMs: 0, maxMs: 0 };
        sections.set(name, section);
    }
    section.count++;
    section.totalMs += durationMs;
    section.maxMs = Math.max(section.maxMs, durationMs);
    frameWorkMs += durationMs;
};

export const measure = ENABLED
    ? (name, fn) => {
        const start = performance.now();
        try {
            return fn();
        } finally {
            const end = performance.now();
            record(name, end - start);
            performance.measure(name, { start, end });
        }
    }
    : (name, fn) => fn();

// For <Profiler onRender>. actualDuration is the time React spent rendering the commit.
export const recordCommit = ENABLED
    ? (id, phase, actualDuration) => record(`react ${phase}`, actualDuration)
    : () => {};

const format = ms => `${ms.toFixed(2)} ms`;

const report = elapsedMs => {
    if (frames.length === 0) return;

    const over = frames.filter(ms => ms > FRAME_BUDGET_MS).length;
    const mean = frames.reduce((sum, ms) => sum + ms, 0) / frames.length;
    const worst = Math.max(...frames);
    console.groupCollapsed(
        `[perf] ${(elapsedMs / 1000).toFixed(1)} s, ${frames.length} frames: script ${format(mean)} a frame, worst ${format(worst)}, ${over} over ${FRAME_BUDGET_MS} ms`
    );
    console.table(Object.fromEntries([...sections].map(([name, s]) => [name, {
        count: s.count,
        'per second': Math.round(s.count / (elapsedMs / 1000)),
        mean: format(s.totalMs / s.count),
        max: format(s.maxMs),
    }])));
    console.groupEnd();

    sections.clear();
    frames = [];
    // User timing entries are kept until cleared; a long run would otherwise hold
    // hundreds of thousands. A recording captures them as they are made.
    performance.clearMeasures();
};

let started = false;

export const startDevPerf = () => {
    if (!ENABLED || started) return;
    started = true;

    // Started before any component mounts, so this callback runs first in each frame
    // and closes the work done since the last one: the previous frame's draw and the
    // socket traffic in between.
    const sample = () => {
        frames.push(frameWorkMs);
        frameWorkMs = 0;
        requestAnimationFrame(sample);
    };
    requestAnimationFrame(sample);

    let lastReport = performance.now();
    setInterval(() => {
        const now = performance.now();
        report(now - lastReport);
        lastReport = now;
    }, REPORT_INTERVAL_MS);

    if (PerformanceObserver.supportedEntryTypes?.includes('long-animation-frame')) {
        new PerformanceObserver(list => {
            list.getEntries().forEach(entry => {
                const scripts = entry.scripts
                    .map(script => `${script.sourceFunctionName || script.invoker} ${format(script.duration)}`)
                    .join(', ');
                console.warn(`[perf] long frame ${format(entry.duration)}, blocking ${format(entry.blockingDuration)}${scripts ? `: ${scripts}` : ''}`);
            });
        }).observe({ type: 'long-animation-frame', buffered: false });
    }

    console.info('[perf] dev instrumentation on; localStorage.setItem(\'adagio:perf\', \'off\') to silence it');
};
