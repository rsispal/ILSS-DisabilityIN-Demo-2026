import type { SVGProps } from 'react';

type IconProps = SVGProps<SVGSVGElement>;

const ic =
  (paths: string[], vb = '0 0 24 24') =>
  (props: IconProps) => (
    <svg
      viewBox={vb}
      fill="none"
      stroke="currentColor"
      strokeWidth={2}
      strokeLinecap="round"
      strokeLinejoin="round"
      {...props}
    >
      {paths.map((d, i) => (
        <path key={i} d={d} />
      ))}
    </svg>
  );

export const PersonIcon = ic([
  'M12 2a4 4 0 0 1 4 4v1a4 4 0 0 1-8 0V6a4 4 0 0 1 4-4Z',
  'M4 21v-1a6 6 0 0 1 6-6h4a6 6 0 0 1 6 6v1',
]);

export const FireIcon = ic([
  'M12 2c1 3 5 5 5 9a5 5 0 0 1-10 0c0-1.5.6-2.7 1.4-3.6C9 9 9.5 7.5 9 6c2 .5 2.5 2 3 4 .8-1.4 0-5 0-8Z',
]);

export const ClearIcon = ic([
  'M5 12h14',
  'M5 12a7 7 0 0 1 7-7 7 7 0 0 1 6 3.5',
  'M19 12a7 7 0 0 1-7 7 7 7 0 0 1-6-3.5',
]);

export const StopCircleIcon = ic([
  'M12 22c5.523 0 10-4.477 10-10S17.523 2 12 2 2 6.477 2 12s4.477 10 10 10Z',
  'M9 9h6v6H9z',
]);

export const ResetIcon = ic([
  'M3 12a9 9 0 0 1 15.5-6.4L21 8',
  'M21 3v5h-5',
  'M21 12a9 9 0 0 1-15.5 6.4L3 16',
  'M3 21v-5h5',
]);

export const AlertTriIcon = ic([
  'M12 3 2 20h20L12 3Z',
  'M12 10v4',
  'M12 17h.01',
]);

export const SoundOnIcon = ic([
  'M11 5 6 9H2v6h4l5 4V5Z',
  'M15.5 8.5a5 5 0 0 1 0 7',
  'M18.5 5.5a9 9 0 0 1 0 13',
]);

export const SoundOffIcon = ic([
  'M11 5 6 9H2v6h4l5 4V5Z',
  'M22 9l-6 6',
  'M16 9l6 6',
]);

export const FlaskIcon = ic([
  'M9 3h6',
  'M10 3v5.4L4.6 18a2 2 0 0 0 1.8 3h11.2a2 2 0 0 0 1.8-3L14 8.4V3',
  'M7.7 14h8.6',
]);

export const BtIcon = (props: IconProps) => (
  <svg
    viewBox="0 0 24 24"
    fill="none"
    stroke="currentColor"
    strokeWidth={2}
    strokeLinecap="round"
    strokeLinejoin="round"
    {...props}
  >
    <path d="M7 8l10 8-5 4V4l5 4-10 8" />
  </svg>
);

export const ChevIcon = (props: IconProps) => (
  <svg
    viewBox="0 0 24 24"
    fill="none"
    stroke="currentColor"
    strokeWidth={2}
    strokeLinecap="round"
    strokeLinejoin="round"
    {...props}
  >
    <path d="M6 9l6 6 6-6" />
  </svg>
);

export const CloseIcon = (props: IconProps) => (
  <svg
    viewBox="0 0 24 24"
    fill="none"
    stroke="currentColor"
    strokeWidth={2}
    strokeLinecap="round"
    strokeLinejoin="round"
    {...props}
  >
    <path d="M6 6l12 12M18 6L6 18" />
  </svg>
);
