/**
 * Analytics is enabled only on the Vercel *production* deployment.
 * Preview deploys, localhost, and local builds never report.
 *
 * `__ILSS_VERCEL_ENV__` is injected at build time from Vercel's `VERCEL_ENV`
 * (see vite.config.ts). Values: `production` | `preview` | `development` | ``.
 */
declare const __ILSS_VERCEL_ENV__: string;

export function isAnalyticsEnabled(): boolean {
  if (__ILSS_VERCEL_ENV__ !== 'production') return false;

  // Runtime guard: never report from a local hostname even if a prod build is served there.
  if (typeof window !== 'undefined') {
    const host = window.location.hostname;
    if (
      host === 'localhost' ||
      host === '127.0.0.1' ||
      host === '[::1]' ||
      host.endsWith('.local')
    ) {
      return false;
    }
  }

  return true;
}
