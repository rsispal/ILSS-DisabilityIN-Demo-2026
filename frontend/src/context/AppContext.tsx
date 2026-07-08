import { createContext, useCallback, useContext, useState, type ReactNode } from 'react';
import { loadProfile, PROFILE_DEFAULTS, saveProfile } from '@/lib/storage';
import type { UserProfile } from '@/types/simulator';

interface AppContextValue {
  profile: UserProfile;
  setProfile: (patch: Partial<UserProfile>) => void;
}

const AppContext = createContext<AppContextValue>({
  profile: PROFILE_DEFAULTS,
  setProfile: () => {},
});

export function AppProvider({ children }: { children: ReactNode }) {
  const [profile, setProfileState] = useState(loadProfile);

  const setProfile = useCallback((patch: Partial<UserProfile>) => {
    setProfileState((p) => {
      const next = { ...p, ...patch };
      saveProfile(next);
      return next;
    });
  }, []);

  return (
    <AppContext.Provider value={{ profile, setProfile }}>{children}</AppContext.Provider>
  );
}

export function useAppContext() {
  return useContext(AppContext);
}
