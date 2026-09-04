// Optional browser QA: NODE_PATH must resolve Playwright, with Chromium installed.
const { chromium } = require('playwright');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const base = process.env.SITE_URL || 'http://127.0.0.1:8519/';
const output = path.resolve('build/website-qa');
fs.mkdirSync(output, { recursive: true });

(async () => {
  const browser = await chromium.launch({ headless: true });
  const page = await browser.newPage({ viewport: { width: 1440, height: 1000 } });
  const errors = [];
  page.on('pageerror', error => errors.push(error.message));
  page.on('response', response => {
    if (response.url().startsWith(base) && response.status() >= 400) errors.push(`${response.status()} ${response.url()}`);
  });
  const results = [];
  async function inspect(route, name, width) {
    await page.setViewportSize({ width, height: width < 500 ? 844 : 1000 });
    await page.goto(new URL(route, base).href, { waitUntil: 'networkidle' });
    const metrics = await page.evaluate(() => ({
      width: innerWidth,
      scrollWidth: document.documentElement.scrollWidth,
      missingImages: [...document.images].filter(i => !i.complete || !i.naturalWidth).map(i => i.src),
      textSize: getComputedStyle(document.querySelector('.md-typeset') || document.body).fontSize
    }));
    assert(metrics.scrollWidth <= metrics.width + 1, `${name}: page overflows horizontally: ${JSON.stringify(metrics)}`);
    assert.deepEqual(metrics.missingImages, [], `${name}: broken image`);
    await page.screenshot({ path: path.join(output, `${name}.png`), fullPage: true });
    if (width < 500) await page.screenshot({ path: path.join(output, `${name}-viewport.png`) });
    results.push({ name, ...metrics });
  }
  await inspect('', 'home-desktop', 1440);
  await page.getByLabel('Find a synth', { exact: true }).fill('nord');
  await page.waitForFunction(() => document.getElementById('synthCountLabel').textContent.startsWith('1 of'));
  assert.match(await page.locator('#synthTableBody').innerText(), /After 2.9.0/);
  await page.getByRole('button', { name: /^Works/ }).click();
  assert.match(await page.locator('#synthTableBody').innerText(), /No matching synths/);
  assert.equal(await page.getByRole('button', { name: /^Works/ }).getAttribute('aria-pressed'), 'true');
  await page.getByRole('button', { name: /^All / }).click();
  await page.getByLabel('Find a synth', { exact: true }).fill('  ROLAND  ');
  assert.match(await page.locator('#synthTableBody').innerText(), /SE-02/);
  await page.getByLabel('Find a synth', { exact: true }).fill('<not-a-synth>');
  assert.match(await page.locator('#synthTableBody').innerText(), /No matching synths/);
  await inspect('docs/learn/import-a-bank/', 'tutorial-desktop', 1440);
  const search = page.getByRole('textbox', { name: 'Search', exact: true });
  await search.fill('fingerprint');
  await page.waitForSelector('.md-search-result__link');
  assert.match(await page.locator('.md-search-result').innerText(), /[Ff]ingerprint/);
  await page.screenshot({ path: path.join(output, 'search-desktop.png') });
  await search.press('Escape');
  await page.locator('.site-header .nav-links').getByRole('link', { name: 'Home', exact: true }).click();
  assert.equal(page.url(), base);
  for (const width of [390, 320]) {
    await inspect('', `home-${width}`, width);
    await page.getByRole('navigation', { name: 'Main navigation' }).getByRole('link', { name: 'Learn', exact: true }).click();
    assert.equal(page.url(), new URL('docs/learn/', base).href);
    await inspect('docs/learn/import-a-bank/', `tutorial-${width}`, width);
    await page.getByRole('button', { name: 'Open documentation contents' }).click();
    await page.locator('label.md-nav__title[for="__nav_4"]').click();
    const mobileHome = page.locator('.md-nav--primary').getByRole('link', { name: 'Home', exact: true });
    await mobileHome.click();
    assert.equal(page.url(), base);
    await inspect('docs/manual/06-lists-and-banks/', `manual-${width}`, width);
    await inspect('docs/help/', `help-${width}`, width);
    await inspect('docs/supported-synths/', `synths-${width}`, width);
  }
  await inspect('docs/manual/06-lists-and-banks/', 'manual-desktop', 1440);
  await inspect('docs/help/', 'help-desktop', 1440);
  await inspect('docs/download/', 'download-desktop', 1440);
  await page.goto(new URL('docs/#/Adaptation%20Programming%20Guide', base).href);
  await page.waitForURL(new URL('docs/programming-guide/', base).href);
  await page.goto(base);
  await page.evaluate(() => { document.documentElement.style.fontSize = '200%'; });
  assert(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth + 1), 'Home text enlargement overflow');
  await page.screenshot({ path: path.join(output, 'home-text-200.png'), fullPage: true });
  const fallback = await browser.newPage();
  await fallback.route('**/data/supported-synths.json', route => route.abort());
  await fallback.goto(base);
  await fallback.waitForFunction(() => document.getElementById('synthCountLabel').textContent.includes('could not load'));
  assert(await fallback.getByRole('link', { name: /Full list, status legend/ }).isVisible());
  const offline = await browser.newPage({ javaScriptEnabled: false });
  await offline.goto(base);
  assert(await offline.getByRole('link', { name: 'complete compatibility list' }).isVisible());
  assert.equal(await offline.getByRole('navigation', { name: 'Main navigation' }).getByRole('link').count(), 7);
  assert.deepEqual(errors, [], 'Browser errors or local failed requests');
  fs.writeFileSync(path.join(output, 'results.json'), JSON.stringify({ results, checks: ['filter search/status/empty state', 'docs search', 'desktop/mobile home links', 'legacy route', '200% text', 'failed fetch fallback', 'no-JS navigation'], errors }, null, 2));
  await browser.close();
  console.log(`Browser checks passed; screenshots and results: ${output}`);
})().catch(error => { console.error(error); process.exit(1); });
