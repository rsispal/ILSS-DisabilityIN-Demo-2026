/**
 * PostHog bootstrap for the ILSS demo.
 *
 * Follows PostHog's React + Vite guide:
 * https://posthog.com/docs/libraries/react
 *
 * Only initialises on Vercel production when a project token is present.
 * Returns the client for `<PostHogProvider client={...}>`, or null when disabled.
 */
import posthog from 'posthog-js';
import type { PostHog } from 'posthog-js';
import { isAnalyticsEnabled } from './isAnalyticsEnabled';

type Props = Record<string, string | number | boolean | null>;

let client: PostHog | null = null;

export function initAnalytics(): PostHog | null {
  if (client) return client;
  if (!isAnalyticsEnabled()) return null;

  const token = import.meta.env.VITE_POSTHOG_PROJECT_TOKEN;
  const apiHost = import.meta.env.VITE_POSTHOG_HOST;
  if (!token || !apiHost) return null;

  try {
    posthog.init(token, {
      api_host: apiHost,
      // Recommended defaults snapshot from PostHog docs (SPA history pageviews, etc.).
      defaults: '2026-05-30',
      // Intentional: only pageviews + ilssAnalytics custom events (no click autocapture).
      autocapture: false,
      disable_session_recording: true,
    });
    client = posthog;
    return client;
  } catch {
    return null;
  }
}

/**
 * Capture via the same initialised singleton used by PostHogProvider.
 * Safe to call from non-React modules (e.g. BleTwinContext); no-ops when disabled.
 */
export function captureEvent(name: string, data?: Props): void {
  if (!isAnalyticsEnabled()) return;
  try {
    const ph = client ?? (posthog.__loaded ? posthog : null);
    if (!ph?.__loaded) return;
    if (data) ph.capture(name, data);
    else ph.capture(name);
  } catch {
    /* never interfere with the app */
  }
}
