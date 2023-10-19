import { ChromiumBrowser, ConsoleMessage, chromium } from 'playwright';
import { expect, test } from '@playwright/test';

import "../assets/data";

let browser!: ChromiumBrowser;

test.describe('Beekeeper factory2 tests', () => {
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

  test('Should be able to init the beekeeper factory2', async ({ page }) => {
    await page.evaluate(async () => {
      const beekeeper = await factory2();

      await beekeeper.createSession("my.salt");

      await beekeeper.delete();
    });
  });

  test('Should be able to get_info based on the created session', async ({ page }) => {
    const info = await page.evaluate(async () => {
      const beekeeper = await factory2();

      const session = await beekeeper.createSession("my.salt");

      return await session.getInfo();
    });

    expect(info).toHaveProperty('now');
    expect(info).toHaveProperty('timeout_time');

    const timeMatch = /\d+-[01]\d-[0-3]\dT[0-2]\d:[0-5]\d:[0-5]\d/;

    expect(info.now).toMatch(timeMatch);
    expect(info.timeout_time).toMatch(timeMatch);
  });

  test('Should be able to create a wallet and import and remove keys', async ({ page }) => {
    const info = await page.evaluate(async () => {
      const beekeeper = await factory2();

      const session = await beekeeper.createSession("my.salt");

      const unlocked = await session.createWallet('w0', 'mypassword');

      await unlocked.importKey('5JkFnXrLM2ap9t3AmAxBJvQHF7xSKtnTrCTginQCkhzU5S7ecPT');
      await unlocked.importKey('5KGKYWMXReJewfj5M29APNMqGEu173DzvHv5TeJAg9SkjUeQV78');

      await unlocked.removeKey('mypassword', '6oR6ckA4TejTWTjatUdbcS98AKETc3rcnQ9dWxmeNiKDzfhBZa');

      return await session.getPublicKeys();
    });

    expect(info).toStrictEqual(['5RqVBAVNp5ufMCetQtvLGLJo7unX9nyCBMMrTXRWQ9i1Zzzizh']);
  });

  test('Should not be able to import keys after closing a wallet', async ({ page }) => {
    const threw = await page.evaluate(async () => {
      const beekeeper = await factory2();

      const session = await beekeeper.createSession("my.salt");

      const unlocked = await session.createWallet('w0', 'mypassword');

      await unlocked.close();

      try {
        await unlocked.importKey('5JkFnXrLM2ap9t3AmAxBJvQHF7xSKtnTrCTginQCkhzU5S7ecPT'); // This should fail

        return false;
      } catch(error) {
        return true;
      }
    });

    expect(threw).toBeTruthy();
  });

  test.afterAll(async () => {
    await browser.close();
  });
});
