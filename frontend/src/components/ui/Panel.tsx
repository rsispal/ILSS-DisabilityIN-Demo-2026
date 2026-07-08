import type { CSSProperties, ReactNode } from 'react';

interface PanelProps {
  eyebrow?: string;
  children: ReactNode;
  style?: CSSProperties;
}

export function Panel({ eyebrow, children, style }: PanelProps) {
  return (
    <div className="panel panel-pad" style={style}>
      {eyebrow && (
        <div className="panel-head">
          <span className="panel-eyebrow">{eyebrow}</span>
          <span className="panel-rule" />
        </div>
      )}
      {children}
    </div>
  );
}
