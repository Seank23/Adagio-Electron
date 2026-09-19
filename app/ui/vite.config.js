import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';
import path from 'path';
import { fileURLToPath } from 'url';

const dirname = path.dirname(fileURLToPath(import.meta.url));

// https://vite.dev/config/
export default defineConfig({
  plugins: [react()],
  base: "./",
  build: {
    outDir: path.resolve(dirname, '../dist/ui'),
    // The outDir sits outside the project root, so Vite leaves it alone unless told
    // otherwise, and electron-builder would then package every stale bundle in it.
    emptyOutDir: true
  },
  server: {
    port: 5173,
    strictPort: true,
  },
  resolve: { dedupe: ['react', 'react-dom'] },
});
