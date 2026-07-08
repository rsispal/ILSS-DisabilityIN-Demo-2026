import { StrictMode } from 'react';
import { createRoot } from 'react-dom/client';
import App from './App';
import './styles/index.css';

// Forge DS components emit React "unique key" dev-warnings from their own
// internal arrays; silence that one upstream message so the console stays clean.
const origError = console.error;
console.error = (...args: unknown[]) => {
  if (typeof args[0] === 'string' && args[0].includes('unique "key" prop')) return;
  origError.apply(console, args);
};

createRoot(document.getElementById('root')!).render(
  <StrictMode>
    <App />
  </StrictMode>,
);
