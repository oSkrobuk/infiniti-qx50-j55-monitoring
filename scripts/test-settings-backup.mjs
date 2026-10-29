// Проверка экспорта и импорта настроек во встроенном Web UI без устройства
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import vm from 'node:vm';

const {chromium} = await import(process.env.KANVEX_PLAYWRIGHT_MODULE || 'playwright');
const source = await readFile(new URL('../src/WebManager.cpp', import.meta.url), 'utf8');
const html = source.match(/static const char INDEX_HTML\[\][\s\S]*?R"rawhtml\(([\s\S]*?)\)rawhtml";/)?.[1];
assert.ok(html);
for (const script of html.matchAll(/<script[^>]*>([\s\S]*?)<\/script>/g)) new vm.Script(script[1]);

const colors = {
  oil: {min: 50, target: 90, max: 98},
  coolant: {min: 50, target: 90, max: 93},
  radiator: {min: 0, target: 50, max: 90},
  transmission: {min: 50, target: 80, max: 98},
  rpm: {green_start: 1000, green_end: 3500, red_start: 4500},
  oil_pressure: {rpm_threshold: 3000, min_low: 1.45, min_high: 3.1},
  boost: {blue_max: 1.3, green_min: 1.58},
  battery: {red_low: 11.5, green_min: 12, green_max: 14.6, red_high: 14.9},
  poll_time: {green_max: 0.2, red_min: 0.5}
};
const alerts = {timing: {confirm_ms: 1000, retrigger_ms: 15000}};
for (let i = 1; i <= 9; i++) alerts[`E0${i}`] = {
  enabled: true, param1: i, param2: 0, param3: 0
};
const config = {...colors, system: {poll_interval_ms: 30, obd_request_spacing_ms: 5,
  stale_ms: 1000, brightness_percent: 100, brightness_night_percent: 30},
  wifi: {ssid: 'private-network', password: 'private-password'}};

const browser = await chromium.launch({headless: true});
try {
  const page = await browser.newPage({viewport: {width: 390, height: 900}, acceptDownloads: true});
  const errors = [];
  const writes = [];
  let failChecks = false;
  page.on('pageerror', error => errors.push(error.message));
  page.on('dialog', dialog => dialog.accept());
  await page.route('**/*', async route => {
    const request = route.request();
    const path = new URL(request.url()).pathname;
    if (path === '/') return route.fulfill({contentType: 'text/html', body: html});
    if (path === '/config' || path === '/checks') {
      if (request.method() === 'POST') {
        writes.push({path, body: request.postDataJSON()});
        if (path === '/checks' && failChecks) {
          failChecks = false;
          return route.fulfill({status: 500, contentType: 'application/json', body: '{}'});
        }
        return route.fulfill({contentType: 'application/json', body: '{"ok":true}'});
      }
      return route.fulfill({contentType: 'application/json',
        body: JSON.stringify(path === '/config' ? config : alerts)});
    }
    return route.fulfill({contentType: 'application/json', body: '{}'});
  });
  await page.goto('http://kanvex.test/');
  await page.locator('#sectSettingsBackup').evaluate(element => { element.open = true; });
  await page.locator('#sectSettingsBackup').screenshot({path: '/tmp/kanvex-settings-backup.png'});
  const downloadPromise = page.waitForEvent('download');
  await page.locator('#btnExportSettings').click();
  const download = await downloadPromise;
  assert.equal(download.suggestedFilename(), 'kanvex-colors-alerts.json');
  const file = JSON.parse(await (await import('node:fs/promises')).readFile(await download.path(), 'utf8'));
  assert.equal(file.format, 'kanvex-colors-alerts');
  assert.deepEqual(file.colors, colors);
  assert.deepEqual(file.alerts, alerts);
  assert.equal(JSON.stringify(file).includes('private-password'), false);
  assert.equal('system' in file.colors, false);

  file.colors.oil.max = 101;
  file.alerts.E01.param1 = 103;
  await page.locator('#settingsFile').setInputFiles({name: 'backup.json',
    mimeType: 'application/json', buffer: Buffer.from(JSON.stringify(file))});
  await page.locator('#btnImportSettings').click();
  await page.waitForFunction(() => document.querySelector('#toast').textContent.includes('импортированы'));
  assert.deepEqual(writes.map(write => write.path), ['/config', '/checks']);
  assert.equal(writes[0].body.oil.max, 101);
  assert.equal(writes[1].body.E01.param1, 103);
  assert.equal('system' in writes[0].body, false);
  assert.equal('wifi' in writes[0].body, false);
  const count = writes.length;
  await page.locator('#settingsFile').setInputFiles({name: 'wrong.json',
    mimeType: 'application/json', buffer: Buffer.from('{"format":"unknown"}')});
  await page.locator('#btnImportSettings').click();
  await page.waitForFunction(() => document.querySelector('#toast').textContent.includes('неподдерживаемый формат'));
  assert.equal(writes.length, count);

  failChecks = true;
  await page.locator('#settingsFile').setInputFiles({name: 'backup.json',
    mimeType: 'application/json', buffer: Buffer.from(JSON.stringify(file))});
  const restored = page.waitForResponse(response =>
    new URL(response.url()).pathname === '/checks' &&
    response.request().method() === 'POST' && response.status() === 200);
  await page.locator('#btnImportSettings').click();
  await restored;
  assert.deepEqual(writes.slice(count).map(write => write.path),
    ['/config', '/checks', '/config', '/checks']);
  assert.deepEqual(writes[count + 2].body, colors);
  assert.deepEqual(writes[count + 3].body, alerts);
  assert.equal(await page.evaluate(() => document.documentElement.scrollWidth > innerWidth), false);
  assert.deepEqual(errors, []);
  await page.close();
} finally { await browser.close(); }
console.log('PASS settings export and import');
