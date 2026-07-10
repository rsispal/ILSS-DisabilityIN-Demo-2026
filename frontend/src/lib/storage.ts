import type { FeatureFlags, UserProfile } from '@/types/simulator';

export const FLAG_KEY = 'ilss-flags';
export const APP_PROFILE_KEY = 'ilss-profile';

export const FLAG_DEFAULTS: FeatureFlags = {
  mouseSwing: false,
  muteByDefault: true,
  deviceLogs: true,
};

export const PROFILE_DEFAULTS: UserProfile = {
  name: 'A. Morgan',
  initials: 'AM',
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

export function loadProfile(): UserProfile {
  try {
    return { ...PROFILE_DEFAULTS, ...JSON.parse(localStorage.getItem(APP_PROFILE_KEY) || '{}') };
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
