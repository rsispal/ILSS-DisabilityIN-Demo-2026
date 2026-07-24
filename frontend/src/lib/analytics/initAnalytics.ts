/**
 * PostHog capture helpers for non-React call sites (e.g. BleTwinContext).
 *
 * Initialisation is owned by `<PostHogProvider apiKey options>` in main.tsx
 * per PostHog's React web/product analytics install guides:
 * https://posthog.com/docs/web-analytics/installation/react
 * https://posthog.com/docs/product-analytics/installation/react
 *
 * Web Analytics + Product Analytics are the same SDK — not separate installs.
 * The dashboard marks them "set up" once `$pageview` / events start arriving.
 */
import posthog from 'posthog-js';
import { isAnalyticsEnabled } from './isAnalyticsEnabled';

type Props = Record<string, string | number | boolean | null>;

/** Capture once the provider has initialised the global client. */
export function captureEvent(name: string, data?: Props): void {
  if (!isAnalyticsEnabled()) return;
  try {
    if (!posthog.__loaded) return;
    if (data) posthog.capture(name, data);
    else posthog.capture(name);
  } catch {
    /* never interfere with the app */
  }
}
