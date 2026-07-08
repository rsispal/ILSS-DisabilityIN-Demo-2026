import { HoneywellLogo } from '@/components/layout/HoneywellLogo';
import { UserAvatarMenu } from '@/components/layout/UserAvatarMenu';

export function AppMenuBar() {
  return (
    <div className="app-menu-bar-wrap">
      <header className="app-menu-bar" id="header">
        <div className="app-menu-bar-brand">
          <HoneywellLogo className="app-menu-bar-logo" />
          <div className="brand-divider" />
          <div className="app-menu-bar-titles">
            <div className="brand-title">Inclusive Life Safety System</div>
            <div className="brand-sub">Smart Lanyard · Simulator</div>
          </div>
        </div>

        <div className="app-menu-bar-right">
          <UserAvatarMenu />
        </div>
      </header>
      <div className="app-menu-bar-spacer" aria-hidden="true" />
    </div>
  );
}
