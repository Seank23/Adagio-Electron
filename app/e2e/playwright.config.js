// End-to-end validation (the overhaul guide's §7): the real Electron app, the real
// engine, the Vite dev server for the renderer. Run from app/ with `npm run test:e2e`.
const { defineConfig } = require('@playwright/test');

module.exports = defineConfig({
    testDir: __dirname,
    testMatch: '*.spec.js',
    // Every test owns port 9001 while it runs, so they go one at a time.
    workers: 1,
    fullyParallel: false,
    timeout: 60000,
    expect: { timeout: 10000 },
    reporter: [['list']],
    outputDir: '../test-results/e2e',
    webServer: {
        command: 'npm run dev',
        cwd: '../ui',
        url: 'http://localhost:5173',
        reuseExistingServer: true,
        timeout: 60000,
    },
});
