import { chromium } from 'playwright';
import { mkdir, writeFile, readdir, rename } from 'fs/promises';
import path from 'path';

const OUT = '/opt/cursor/artifacts/verification';
const BASE = process.env.BASE_URL || 'http://127.0.0.1:4173';

const VIEWPORTS = {
  desktop: { width: 1440, height: 900, name: 'desktop' },
  tablet: { width: 834, height: 1112, name: 'tablet' },
  mobile: { width: 390, height: 844, name: 'mobile' },
};

async function waitForApp(page) {
  await page.goto(BASE, { waitUntil: 'networkidle', timeout: 30000 });
  await page.waitForSelector('.brand-title', { timeout: 15000 });
  await page.waitForSelector('.scene .badge', { timeout: 15000 });
  await page.waitForSelector('text=Emergency Controls', { timeout: 15000 });
}

async function exercise(page) {
  await page.getByRole('button', { name: /Simulate Personal Alert/i }).click();
  await page.waitForTimeout(1200);
  await page.getByRole('button', { name: /Clear Personal Alert/i }).click();
  await page.waitForTimeout(800);
  await page.getByRole('button', { name: /Simulate Fire Emergency/i }).click();
  await page.waitForTimeout(1200);
  await page.getByRole('button', { name: /Clear Fire Emergency/i }).click();
  await page.waitForTimeout(800);
}

async function renameVideo(dir, viewportName) {
  const files = await readdir(dir);
  const webm = files.find((f) => f.endsWith('.webm'));
  if (webm) {
    await rename(path.join(dir, webm), path.join(dir, `${viewportName}-walkthrough.webm`));
  }
}

async function recordViewport(browser, key) {
  const vp = VIEWPORTS[key];
  const dir = path.join(OUT, vp.name);
  await mkdir(dir, { recursive: true });

  const context = await browser.newContext({
    viewport: { width: vp.width, height: vp.height },
    recordVideo: { dir, size: { width: vp.width, height: vp.height } },
  });

  const page = await context.newPage();
  const errors = [];
  page.on('pageerror', (err) => errors.push(err.message));
  page.on('console', (msg) => {
    if (msg.type() === 'error') errors.push(msg.text());
  });

  try {
    await waitForApp(page);
    await page.screenshot({ path: path.join(dir, '01-loaded.png'), fullPage: true });
    await exercise(page);
    await page.screenshot({ path: path.join(dir, '02-after-alerts.png'), fullPage: true });
    await page.getByRole('button', { name: /Experiments/i }).click();
    await page.waitForSelector('text=Feature flags', { timeout: 5000 });
    await page.screenshot({ path: path.join(dir, '03-experiments-modal.png'), fullPage: true });
    await page.waitForTimeout(1000);
  } finally {
    await context.close();
    await renameVideo(dir, vp.name);
  }

  return { viewport: vp.name, errors };
}

async function main() {
  await mkdir(OUT, { recursive: true });
  const browser = await chromium.launch({ headless: true });
  const results = [];

  for (const key of Object.keys(VIEWPORTS)) {
    results.push(await recordViewport(browser, key));
  }

  await browser.close();

  const report = {
    baseUrl: BASE,
    results,
    passed: results.every((r) => r.errors.length === 0),
  };

  await writeFile(path.join(OUT, 'report.json'), JSON.stringify(report, null, 2));
  console.log(JSON.stringify(report, null, 2));
  if (!report.passed) process.exit(1);
}

main().catch((err) => {
  console.error(err);
  process.exit(1);
});
