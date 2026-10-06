import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';
import path from 'path';
import { readFileSync } from 'fs';
import { fileURLToPath } from 'url';

const dirname = path.dirname(fileURLToPath(import.meta.url));
// The wire protocol is described once, at the repo root, and read by both sides: the
// engine generates a header from it, the UI imports it through this alias.
const protocolDir = path.resolve(dirname, '../../protocol');
// The app's version is app/package.json's, not app/ui's.
const appPackage = JSON.parse(readFileSync(path.resolve(dirname, '../package.json'), 'utf8'));

// https://vite.dev/config/
export default defineConfig({
  plugins: [react()],
  base: "./",
  // Preferences' About section. The local date, not toISOString's UTC one, which is the day
  // before for a build just after midnight in summer time. A dev session shows the day it started.
  define: {
    __APP_VERSION__: JSON.stringify(appPackage.version),
    __BUILD_DATE__: JSON.stringify(new Date().toLocaleDateString('en-CA')),
  },
  build: {
    outDir: path.resolve(dirname, '../dist/ui'),
    // The outDir sits outside the project root, so Vite leaves it alone unless told
    // otherwise, and electron-builder would then package every stale bundle in it.
    emptyOutDir: true
  },
  server: {
    port: 5173,
    strictPort: true,
    // protocol/ sits outside the Vite root, so the dev server has to be told it may
    // serve it. The production build inlines the JSON and needs no equivalent.
    fs: { allow: [dirname, protocolDir] },
  },
  resolve: {
    dedupe: ['react', 'react-dom'],
    alias: { '@protocol': protocolDir },
  },
  // Vitest: unit and component tests under test/, run in jsdom against the real modules.
  test: {
    environment: 'jsdom',
    include: ['test/**/*.test.jsx'],
    setupFiles: ['test/setup.jsx'],
    restoreMocks: true,
  },
});
