import type { FeatureFlags, UserProfile } from '@/types/simulator';

export const FLAG_KEY = 'ilss-flags';
export const APP_PROFILE_KEY = 'ilss-profile';

export const FLAG_DEFAULTS: FeatureFlags = {
  mouseSwing: false,
  muteByDefault: true,
  deviceLogs: true,
};

export const PROFILE_DEFAULTS: UserProfile = {
  name: 'Rav Sispal',
  initials: 'RS',
};

export function loadFlags(): FeatureFlags {
  try {
    return { ...FLAG_DEFAULTS, ...JSON.parse(localStorage.getItem(FLAG_KEY) || '{}') };
  } catch {
    return { ...FLAG_DEFAULTS };
  }
}

export function saveFlags(flags: FeatureFlags) {
  try {
    localStorage.setItem(FLAG_KEY, JSON.stringify(flags));
  } catch {
    /* ignore */
  }
}

/** Previous demo defaults — rewrite once so the avatar menu shows the new identity. */
const LEGACY_DEMO_NAMES = new Set(['A. Morgan', 'Amy Morgan', 'A Morgan']);

export function loadProfile(): UserProfile {
  try {
    const raw = localStorage.getItem(APP_PROFILE_KEY);
    if (!raw) return { ...PROFILE_DEFAULTS };
    const stored = JSON.parse(raw) as Partial<UserProfile>;
    const name = (stored.name || '').trim();
    const initials = (stored.initials || '').trim().toUpperCase();
    // Migrate only the old demo identity (not a custom "AM" user with another name).
    if (LEGACY_DEMO_NAMES.has(name) || (name === '' && initials === 'AM')) {
      const next = { ...PROFILE_DEFAULTS };
      saveProfile(next);
      return next;
    }
    return { ...PROFILE_DEFAULTS, ...stored };
  } catch {
    return { ...PROFILE_DEFAULTS };
  }
}

export function saveProfile(profile: UserProfile) {
  try {
    localStorage.setItem(APP_PROFILE_KEY, JSON.stringify(profile));
  } catch {
    /* ignore */
  }
}
