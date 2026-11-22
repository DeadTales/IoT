import { svelte } from '@sveltejs/vite-plugin-svelte';
import { defineConfig, loadEnv } from 'vite';

export default defineConfig(({ mode }) => {
  const { API_TARGET = 'http://127.0.0.1:8000' } = loadEnv(mode, process.cwd(), 'API_');
  // El proxy mantiene las peticiones y la cookie de sesión en el mismo origen.
  const proxy = { '/api': API_TARGET };

  return { plugins: [svelte()], server: { proxy }, preview: { proxy } };
});
