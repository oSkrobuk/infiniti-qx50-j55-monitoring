// Проверка встроенного Web UI с подменой HTTP API: физическое устройство не используется
import assert from 'node:assert/strict';
import { readFile, mkdir } from 'node:fs/promises';
import { resolve } from 'node:path';
import vm from 'node:vm';

const { chromium } = await import(process.env.KANVEX_PLAYWRIGHT_MODULE || 'playwright');
const source = await readFile(new URL('../src/WebManager.cpp', import.meta.url), 'utf8');
const html = source.match(/static const char INDEX_HTML\[\][\s\S]*?R"rawhtml\(([\s\S]*?)\)rawhtml";/)?.[1];
assert.ok(html, 'INDEX_HTML missing');
for (const script of html.matchAll(/<script[^>]*>([\s\S]*?)<\/script>/g)) new vm.Script(script[1]);
const artifacts = resolve(process.env.KANVEX_UI_ARTIFACTS || '/tmp/kanvex-ble-ui');
await mkdir(artifacts, {recursive: true});
const browser = await chromium.launch({headless: true});
try {
  for (const width of [390, 1200]) {
    const page = await browser.newPage({viewport: {width, height: 900}});
    let paired = true;
    let failStatus = false;
    let resetStatus = 200;
    const posts = [];
    const errors = [];
    page.on('pageerror', error => errors.push(error.message));
    await page.route('**/*', async route => {
      const request = route.request();
      const url = new URL(request.url());
      if (url.hostname !== 'kanvex.test') return route.fulfill({status: 404, body: '{}'});
      if (url.pathname === '/') return route.fulfill({contentType: 'text/html', body: html});
      if (url.pathname === '/ble') return route.fulfill({status: failStatus ? 500 : 200,
        contentType: 'application/json', body: JSON.stringify({name: 'QX50M_7a3f', paired,
          ownerId: paired ? '18ed6e3fe03146a2' : '', connected: false, authenticated: false,
          history: {capacity: 1800, stored: 157, unread: 10, oldestSeq: 0, nextSeq: 157,
            ackedUpto: 147, lostUnacked: 0}, summaries: {stored: 2, unread: 1}})});
      if (url.pathname === '/ble/forget-phone') {
        posts.push(request.headers()['x-kanvex-reset']);
        assert.equal(request.method(), 'POST');
        assert.equal(posts.at(-1), '18ed6e3fe03146a2');
        if (resetStatus === 200) paired = false;
        return route.fulfill({status: resetStatus, contentType: 'application/json', body: '{}'});
      }
      return route.fulfill({contentType: 'application/json', body: '{}'});
    });
    await page.goto('http://kanvex.test/');
    await page.locator('#sectSystem').evaluate(element => { element.open = true; });
    await page.waitForFunction(() => document.querySelector('#bleOwner').textContent.includes('18ed6e3fe03146a2'));
    assert.equal(await page.locator('#bleName input').count(), 0);
    assert.equal(await page.locator('#bleName').innerText(), 'QX50M_7a3f');
    assert.equal(await page.locator('#bleBufferUsed').innerText(), '157 / 1800');
    assert.equal(await page.locator('#bleBufferUnread').innerText(), '10');
    assert.equal(await page.locator('#bleBufferFree').innerText(), '1643');
    assert.match(await page.locator('#bleBufferDetail').innerText(), /Курсор подтверждения: № 147/);
    assert.match(await page.locator('#bleBufferDetail').innerText(), /1 без подтверждения из 2/);
    assert.equal(await page.locator('#bleBufferMeter').evaluate(el => el.value), 157);
    assert.equal(await page.evaluate(() => document.documentElement.scrollWidth > innerWidth), false);
    await page.locator('.ble-identity').screenshot({path: `${artifacts}/ble-${width}-paired.png`});
    page.once('dialog', dialog => dialog.dismiss());
    await page.locator('#btnForgetPhone').click();
    assert.equal(posts.length, 0);
    resetStatus = 409;
    page.once('dialog', dialog => dialog.accept());
    await page.locator('#btnForgetPhone').click();
    await page.waitForFunction(() => document.querySelector('#bleMessage').textContent.includes('Привязка изменилась'));
    assert.equal(paired, true);
    resetStatus = 200;
    page.once('dialog', dialog => dialog.accept());
    await page.locator('#btnForgetPhone').click();
    await page.waitForFunction(() => document.querySelector('#bleOwner').textContent === 'Телефон не привязан');
    assert.equal(await page.locator('#btnForgetPhone').isDisabled(), true);
    await page.locator('.ble-identity').screenshot({path: `${artifacts}/ble-${width}-empty.png`});
    failStatus = true;
    await page.evaluate(() => loadBle());
    assert.equal(await page.locator('#btnForgetPhone').isDisabled(), true);
    assert.match(await page.locator('#bleMessage').innerText(), /недоступно/);
    assert.deepEqual(errors, []);
    await page.close();
    console.log(`PASS ${width}px: readonly name, confirmation, stale binding, removal, empty and error states`);
  }
} finally { await browser.close(); }
console.log(`Screenshots: ${artifacts}`);
