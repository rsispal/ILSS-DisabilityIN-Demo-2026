# ILSS Lanyard Simulator

React + TypeScript + Vite port of the ILSS Smart Lanyard Simulator prototype.

## Prerequisites

- Node.js 18+
- npm

## Development

```bash
npm install
npm run dev
```

Open http://localhost:5173

## Production build

```bash
npm run build
npm run preview
```

The build outputs to `dist/`, including the Forge design system bundle (`public/_ds`) and static assets.

## Design system

Honeywell Forge Common UI (`@forge/common`) is loaded as a browser global from `public/_ds/`. Components are accessed via `src/ds/forge.ts`, which re-exports `window.ForgeCommon` with TypeScript types.

## Project structure

```
src/
  pages/LanyardSimulatorPage.tsx   # Main simulator screen
  components/                      # Layout, controls, lanyard, BLE, experiments
  hooks/                           # Simulator state, feature flags, audio
  lib/                             # Audio synthesis, constants, storage
  styles/                          # App CSS (split by concern)
  ds/                              # Typed Forge DS wrapper
public/
  _ds/                             # Forge design system bundle
  uploads/                         # Static assets (e.g. embossed logo SVG)
```

## Feature flags (localStorage)

- `ilss-flags` — mouse swing physics, mute-by-default
- `ilss-profile` — display name and avatar initials
