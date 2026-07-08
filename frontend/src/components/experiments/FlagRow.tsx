interface FlagRowProps {
  id: string;
  title: string;
  desc: string;
  badge?: string;
  checked: boolean;
  onChange: (checked: boolean) => void;
}

export function FlagRow({ id, title, desc, badge, checked, onChange }: FlagRowProps) {
  return (
    <div className="flag-row">
      <div className="flag-text">
        <div className="flag-title">
          {title}
          {badge && <span className="flag-badge">{badge}</span>}
        </div>
        <div className="flag-desc">{desc}</div>
      </div>
      <label className="flag-toggle" htmlFor={id}>
        <input
          type="checkbox"
          id={id}
          checked={checked}
          onChange={(ev) => onChange(ev.target.checked)}
        />
        <span className="flag-toggle-track">
          <span className="flag-toggle-thumb" />
        </span>
      </label>
    </div>
  );
}
