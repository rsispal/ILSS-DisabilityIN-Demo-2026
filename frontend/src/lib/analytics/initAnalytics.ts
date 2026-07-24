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

const LANYARD_SERIAL_KEY = 'lanyard_serial';

/** Current paired device serial — merged into every custom event. */
let lanyardSerial: string | null = null;

function withDeviceProps(data?: Props): Props | undefined {
  if (!lanyardSerial && !data) return undefined;
  return {
    ...(data ?? {}),
    [LANYARD_SERIAL_KEY]: lanyardSerial,
  };
}

/**
 * Attach / clear the paired lanyard serial on all subsequent PostHog events
 * (custom + autocapture) via super-properties, and for explicit captures.
 */
export function setLanyardSerial(serial: string | null): void {
  lanyardSerial = serial && serial.trim() ? serial.trim() : null;
  if (!isAnalyticsEnabled()) return;
  try {
    if (!posthog.__loaded) return;
    if (lanyardSerial) {
      posthog.register({ [LANYARD_SERIAL_KEY]: lanyardSerial });
    } else {
      posthog.unregister(LANYARD_SERIAL_KEY);
    }
  } catch {
    /* never interfere with the app */
  }
}

/** Capture once the provider has initialised the global client. */
export function captureEvent(name: string, data?: Props): void {
  if (!isAnalyticsEnabled()) return;
  try {
    if (!posthog.__loaded) return;
    const props = withDeviceProps(data);
    if (props) posthog.capture(name, props);
    else posthog.capture(name);
  } catch {
    /* never interfere with the app */
  }
}
