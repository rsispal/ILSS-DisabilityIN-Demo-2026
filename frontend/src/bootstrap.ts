/**
 * Bootstrap: expose npm React on window for ForgeCommon, load DS bundle, then start app.
 * The DS bundle is built against UMD React globals — without this, the app renders a blank screen.
 */
import React from 'react';
import * as ReactDOM from 'react-dom';
import { createRoot } from 'react-dom/client';

declare global {
  interface Window {
    React: typeof React;
    ReactDOM: typeof ReactDOM & { createRoot: typeof createRoot };
  }
}

window.React = React;
window.ReactDOM = { ...ReactDOM, createRoot };

const DS_SCRIPT = '/_ds/forge-common-ui-library-a68de1eb-0d1a-4330-b96f-9932154b0b27/_ds_bundle.js';

function loadScript(src: string): Promise<void> {
  return new Promise((resolve, reject) => {
    if (document.querySelector(`script[data-src="${src}"]`)) {
      resolve();
      return;
    }
    const el = document.createElement('script');
    el.src = src;
    el.dataset.src = src;
    el.onload = () => resolve();
    el.onerror = () => reject(new Error(`Failed to load ${src}`));
    document.head.appendChild(el);
  });
}

async function bootstrap() {
  await loadScript(DS_SCRIPT);
  await import('./main');
}

bootstrap().catch((err) => {
  console.error('Bootstrap failed:', err);
  const root = document.getElementById('root');
  if (root) {
    root.innerHTML = `<div style="padding:24px;color:#ff9a93;font-family:system-ui">
      <h2>Failed to start simulator</h2><pre>${String(err)}</pre></div>`;
  }
});
