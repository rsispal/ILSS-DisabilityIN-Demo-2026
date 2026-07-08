import type { ComponentType, SVGProps } from 'react';

interface AlertButtonProps {
  kind: 'alert' | 'danger' | 'clear';
  armed?: boolean;
  disabled?: boolean;
  Icon: ComponentType<SVGProps<SVGSVGElement>>;
  title: string;
  desc: string;
  onClick: () => void;
}

export function AlertButton({
  kind,
  armed,
  disabled,
  Icon,
  title,
  desc,
  onClick,
}: AlertButtonProps) {
  return (
    <button
      type="button"
      className={`alert-btn ${kind}` + (armed ? ' armed' : '')}
      disabled={disabled}
      onClick={onClick}
    >
      <span className="ab-icon">
        <Icon />
      </span>
      <span className="ab-text">
        <span className="ab-title">{title}</span>
        <span className="ab-desc">{desc}</span>
      </span>
    </button>
  );
}
