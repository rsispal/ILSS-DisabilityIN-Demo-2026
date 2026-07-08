interface ChipItem {
  v: string;
  l: string;
}

interface ChipsProps {
  items: ChipItem[];
  value: string | null;
  onChange: (value: string) => void;
}

export function Chips({ items, value, onChange }: ChipsProps) {
  return (
    <div className="chips">
      {items.map((it) => (
        <button
          key={it.v}
          type="button"
          className={'chip-btn' + (value === it.v ? ' sel' : '')}
          onClick={() => onChange(it.v)}
        >
          {it.l}
        </button>
      ))}
    </div>
  );
}
