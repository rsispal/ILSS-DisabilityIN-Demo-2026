import { StrictMode } from 'react';
import { createRoot } from 'react-dom/client';
import { PostHogProvider } from '@posthog/react';
import App from './App';
import { ErrorBoundary } from './components/ErrorBoundary';
import { isAnalyticsEnabled } from './lib/analytics/isAnalyticsEnabled';
import './styles/index.css';
import './styles/device-logs.css';

// Forge DS components emit React "unique key" dev-warnings from their own
// internal arrays; silence that one upstream message so the console stays clean.
const origError = console.error;
console.error = (...args: unknown[]) => {
  if (typeof args[0] === 'string' && args[0].includes('unique "key" prop')) return;
  origError.apply(console, args);
};

/**
 * PostHog React install (web + product analytics share this setup):
 * https://posthog.com/docs/web-analytics/installation/react
 *
 * Only mount on Vercel production with both env vars present.
 * Autocapture stays on (PostHog default) so Web Analytics can populate.
 * Session replay stays off to avoid burning free-tier recording quota.
 */
const token = import.meta.env.VITE_POSTHOG_PROJECT_TOKEN;
const apiHost = import.meta.env.VITE_POSTHOG_HOST;
const analyticsEnabled = isAnalyticsEnabled() && Boolean(token && apiHost);

const posthogOptions = {
  api_host: apiHost,
  defaults: '2026-05-30',
  disable_session_recording: true,
} as const;

createRoot(document.getElementById('root')!).render(
  <StrictMode>
    <ErrorBoundary>
      {analyticsEnabled ? (
        <PostHogProvider apiKey={token!} options={posthogOptions}>
          <App />
        </PostHogProvider>
      ) : (
        <App />
      )}
    </ErrorBoundary>
  </StrictMode>,
);
