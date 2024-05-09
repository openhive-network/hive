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

      beekeeper.createSession("my.salt");

      await beekeeper.delete();
    });
  });

  test('Should be able to get_info based on the created session', async ({ page }) => {
    const info = await page.evaluate(async () => {
      const beekeeper = await factory();

      const session = beekeeper.createSession("my.salt");

      return session.getInfo();
    });

    expect(info).toHaveProperty('now');
    expect(info).toHaveProperty('timeout_time');

    const timeMatch = /\d+-[01]\d-[0-3]\dT[0-2]\d:[0-5]\d:[0-5]\d/;

    expect(info.now).toMatch(timeMatch);
    expect(info.timeout_time).toMatch(timeMatch);
  });

  test('Should set a proper timeout', async ({ page }) => {
    const timeoutSeconds = 5;

    const info = await page.evaluate(async unlockTimeout => {
      const beekeeper = await factory({
        unlockTimeout
      });

      const session = beekeeper.createSession("my.salt");

      return session.getInfo();
    }, timeoutSeconds);

    expect(new Date(info.timeout_time).getTime()).toBe(new Date(info.now).getTime() + (timeoutSeconds * 1000 ));
  });

  test('Should be able to create multiple sessions with wallets and delete the beekeeper instance', async ({ page }) => {
    await page.evaluate(async () => {
      const beekeeper = await factory();

      const session1 = beekeeper.createSession("avocado1");
      const session2 = beekeeper.createSession("avocado2");

      await session1.createWallet('w0');
      await session2.createWallet('w1');

      await beekeeper.delete();
    });
  });

  test('Should be able to create a wallet and import and remove keys', async ({ page }) => {
    const info = await page.evaluate(async () => {
      const beekeeper = await factory();

      const session = beekeeper.createSession("my.salt");

      const { wallet: unlocked } = await session.createWallet('w0', 'mypassword');

      await unlocked.importKey('5JkFnXrLM2ap9t3AmAxBJvQHF7xSKtnTrCTginQCkhzU5S7ecPT');
      await unlocked.importKey('5KGKYWMXReJewfj5M29APNMqGEu173DzvHv5TeJAg9SkjUeQV78');

      await unlocked.removeKey('STM6oR6ckA4TejTWTjatUdbcS98AKETc3rcnQ9dWxmeNiKDzfhBZa');

      return unlocked.getPublicKeys();
    });

    expect(info).toStrictEqual(['STM5RqVBAVNp5ufMCetQtvLGLJo7unX9nyCBMMrTXRWQ9i1Zzzizh']);
  });

  test('Should not be able to import keys after closing a wallet', async ({ page }) => {
    const threw = await page.evaluate(async () => {
      const beekeeper = await factory();

      const session = beekeeper.createSession("my.salt");

      const { wallet: unlocked } = await session.createWallet('w0', 'mypassword');

      unlocked.close();

      try {
        await unlocked.importKey('5JkFnXrLM2ap9t3AmAxBJvQHF7xSKtnTrCTginQCkhzU5S7ecPT'); // This should fail

        return false;
      } catch(error) {
        return true;
      }
    });

    expect(threw).toBeTruthy();
  });

  test('Should be able to create multiple wallets and access them using listWallets references', async ({ page }) => {
    const info = await page.evaluate(async () => {
      const beekeeper = await factory();

      const session = beekeeper.createSession("my.salt");

      await session.createWallet('w0', 'mypassword');
      await session.createWallet('w1', 'mypassword');
      await session.createWallet('w2', 'mypassword');

      return session.listWallets().map(value => value.name);
    });

    expect(info).toStrictEqual(['w0','w1','w2']);
  });

  test('Shold be able to encrypt and decrypt messages', async ({ page }) => {
    const input = "Big Brother is Watching You";

    const retVal = await page.evaluate(async (input) => {
      const beekeeper = await factory();

      const session = beekeeper.createSession("my.salt");

      const { wallet } = await session.createWallet('w0', 'mypassword');

      const inputKey = await wallet.importKey('5KLytoW1AiGSoHHBA73x1AmgZnN16QDgU1SPpG9Vd2dpdiBgSYw');
      const outputKey = await wallet.importKey('5KXNQP5feaaXpp28yRrGaFeNYZT7Vrb1PqLEyo7E3pJiG1veLKG');

      const encrypted = wallet.encryptData(input, inputKey, outputKey);

      return wallet.decryptData(encrypted, inputKey, outputKey);
    }, input);

    expect(retVal).toBe(input);
  });

  test.afterAll(async () => {
    await browser.close();
  });
});
