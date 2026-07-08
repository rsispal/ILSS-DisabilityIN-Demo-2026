import type { ColorKey } from '@/types/simulator';

export const COLORS: Record<ColorKey, { rgb: string; label: string }> = {
  red: { rgb: '255, 49, 49', label: 'Red' },
  green: { rgb: '43, 226, 122', label: 'Green' },
  blue: { rgb: '56, 132, 255', label: 'Blue' },
  teal: { rgb: '22, 224, 208', label: 'Teal' },
  purple: { rgb: '178, 84, 255', label: 'Purple' },
  yellow: { rgb: '255, 210, 40', label: 'Yellow' },
  orange: { rgb: '255, 138, 36', label: 'Orange' },
};
