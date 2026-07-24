/**
 * Self-contained PostHog analytics facade for the ILSS demo.
 *
 * Web Analytics (pageviews / traffic) and Product Analytics (custom events) both
 * come from the same posthog-js + PostHogProvider setup in main.tsx — there are
 * no separate “web” / “product” packages to install.
 *
 * Custom events go through `captureEvent` on the provider-initialised client.
 * Call sites never throw into app logic.
 *
 * Reporting is gated to the Vercel production deployment only (not preview, not localhost).
 * We skip `identify()` — this demo has no authenticated user IDs (PostHog anon distinct_id is fine).
 */
import { captureEvent } from './initAnalytics';

type Props = Record<string, string | number | boolean | null>;

function safeTrack(name: string, data?: Props): void {
  captureEvent(name, data);
}

export type PairingIssueReason = 'error' | 'timeout' | 'cancelled' | 'unsupported';

/**
 * Typed analytics helpers for demo interactions.
 * Event names are stable — rename carefully (they appear in the PostHog dashboard).
 */
export const ilssAnalytics = {
  /** Lanyard BLE (or simulate) pairing completed successfully. */
  pairingSucceeded(data?: { simulated?: boolean; name?: string | null }) {
    safeTrack('Lanyard Pairing Succeeded', {
      simulated: data?.simulated ?? false,
      name: data?.name ?? null,
    });
  },

  /** Pairing failed, timed out, was cancelled, or is unsupported. */
  pairingIssue(data: { reason: PairingIssueReason; message?: string | null }) {
    safeTrack('Lanyard Pairing Issue', {
      reason: data.reason,
      message: data.message ? data.message.slice(0, 255) : null,
    });
  },

  /** User ran the fire emergency simulation from the web UI. */
  fireSimulation() {
    safeTrack('Fire Simulation');
  },

  /** User ran the personal alert simulation from the web UI / browser. */
  personalAlertSimulation() {
    safeTrack('Personal Alert Simulation');
  },

  /** Personal alert state arrived from the physical lanyard. */
  personalAlertFromLanyard() {
    safeTrack('Personal Alert From Lanyard');
  },

  /** User activated a custom Edge LED colour / pattern / brightness. */
  customLed(data?: { color?: string; pattern?: string; brightness?: number }) {
    safeTrack('Custom LED', {
      color: data?.color ?? null,
      pattern: data?.pattern ?? null,
      brightness: data?.brightness ?? null,
    });
  },

  /** User activated a custom piezo / buzzer pattern on the twin. */
  customBuzzer(data?: { pattern?: string }) {
    safeTrack('Custom Buzzer', {
      pattern: data?.pattern ?? null,
    });
  },

  /**
   * Browser Web Audio started sounding a pattern (audio indication path).
   * Distinct from Custom Buzzer (device twin pattern selection).
   */
  customSound(data?: { pattern?: string }) {
    safeTrack('Custom Sound', {
      pattern: data?.pattern ?? null,
    });
  },

  /** Browser audio indication (mute) toggled. */
  audioIndication(enabled: boolean) {
    safeTrack('Audio Indication', {
      enabled,
    });
  },

  /** Device Logs pane was opened / viewed. */
  logsViewed() {
    safeTrack('Logs Viewed');
  },
} as const;
