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
    outDir: path.resolve(dirname, '../dist/ui')
  },
  server: {
    port: 5173,
    strictPort: true,
  },
  resolve: { dedupe: ['react', 'react-dom'] },
});
