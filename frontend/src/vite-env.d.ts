/// <reference types="vite/client" />

/** Injected by vite.config.ts from `process.env.VERCEL_ENV`. */
declare const __ILSS_VERCEL_ENV__: string;

interface ImportMetaEnv {
  readonly VITE_POSTHOG_PROJECT_TOKEN?: string;
  readonly VITE_POSTHOG_HOST?: string;
}
