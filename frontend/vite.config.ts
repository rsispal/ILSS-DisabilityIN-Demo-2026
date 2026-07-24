import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';
import path from 'path';

export default defineConfig({
  plugins: [react()],
  root: '.',
  publicDir: 'public',
  resolve: {
    alias: {
      '@': path.resolve(__dirname, 'src'),
    },
  },
  // Bake Vercel's deployment target into the client bundle so analytics can
  // distinguish production vs preview (NODE_ENV is "production" for both).
  // Use a dedicated global — defining import.meta.env.VITE_* is unreliable with Vite.
  define: {
    __ILSS_VERCEL_ENV__: JSON.stringify(process.env.VERCEL_ENV ?? ''),
  },
  server: {
    port: 5173,
  },
});
