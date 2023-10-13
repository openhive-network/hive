import { ChromiumBrowser, ConsoleMessage, chromium } from 'playwright';
import { expect, test } from '@playwright/test';

import "../assets/data";

let browser!: ChromiumBrowser;

test.describe('Beekeeper factory tests', () => {
  test.beforeAll(async () => {
    browser = await chromium.launch({
      headless: true
    });
  });

  test.beforeEach(async({ page }) => {
    page.on('console', (msg: ConsoleMessage) => {
      console.log('>>', msg.type(), msg.text())
    });

    await page.goto(`http://localhost:8080/__tests__/assets/test.html`);
    await page.waitForURL('**/test.html', { waitUntil: 'load' });
  });

  test('Should be able to init the beekeeper factory', async ({ page }) => {
    await page.evaluate(async () => {
      const beekeeper = await factory();

      beekeeper.delete();
    });
  });

  test('Should be able to get_info based on the created session', async ({ page }) => {
    const info = await page.evaluate(async () => {
      const beekeeper = await factory();

      const sessionData = await beekeeper.create_session('pear') as { token: string };

      return beekeeper.get_info(sessionData.token);
    }) as { now: string; timeout_time: string };

    expect(info).toHaveProperty('now');
    expect(info).toHaveProperty('timeout_time');

    const timeMatch = /\d+-[01]\d-[0-3]\dT[0-2]\d:[0-5]\d:[0-5]\d/;

    expect(info.now).toMatch(timeMatch);
    expect(info.timeout_time).toMatch(timeMatch);
  });

  test.afterAll(async () => {
    await browser.close();
  });
});
