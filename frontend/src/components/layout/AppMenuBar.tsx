import { Logo, Button } from '@/ds/forge';
import { UserAvatarMenu } from '@/components/layout/UserAvatarMenu';

function MenuIcon() {
  return (
    <svg
      width="24"
      height="24"
      viewBox="0 0 24 24"
      fill="none"
      stroke="currentColor"
      strokeWidth="1.5"
      strokeLinecap="round"
      aria-hidden
    >
      <path d="M4 7h16M4 12h16M4 17h16" />
    </svg>
  );
}

export function AppMenuBar() {
  return (
    <div className="app-menu-bar-wrap">
      <header className="app-menu-bar" id="header">
        <div className="app-menu-bar-toggle">
          <Button
            variant="quiet-secondary"
            icon={<MenuIcon />}
            aria-label="Toggle navigation panel"
            onClick={() => {}}
          />
        </div>

        <div className="app-menu-bar-brand">
          <Logo icon={false} className="app-menu-bar-logo" />
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
