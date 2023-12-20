import { ChromiumBrowser, ConsoleMessage, chromium } from 'playwright';
import { test, expect } from '@playwright/test';

import { WALLET_OPTIONS } from '../assets/data';

let browser!: ChromiumBrowser;

test.describe('WASM Base tests', () => {
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

  // Base browser type test
  test('Should test on chromium', async () => {
    const browserType = browser.browserType();
    const version = browser.version();

    console.info(`Using browser ${browserType} v${version}`);;

    expect(browserType.name()).toBe('chromium');
  });

  // Base valid test html webpage test
  test('Should have a valid html test webpage', async ({ page }) => {
    const id = await page.$eval("body", n => n.getAttribute("id"))

    expect(id).toBe('beekeeperbody');
  });

  test('Should have global module', async ({ page }) => {
    const moduleType = await page.evaluate(() => {
      return typeof beekeeper;
    });

    expect(moduleType).toBe('function');
  });

  test('Should be able to create instance of StringList', async ({ page }) => {
    await page.evaluate(async () => {
      const provider = await beekeeper();
      new provider.StringList();
    });
  });

  test('Should be able to create instance of beekeeper_api', async ({ page }) => {
    await page.evaluate(async (WALLET_OPTIONS) => {
      const provider = await beekeeper();
      const beekeeperOptions = new provider.StringList();
      WALLET_OPTIONS.forEach((opt) => void beekeeperOptions.push_back(opt));

      new provider.beekeeper_api(beekeeperOptions);
    }, WALLET_OPTIONS);
  });

  test('Should be able to create instance of ExtractError - import script', async ({ page }) => {
    await page.evaluate(async () => {
      new ExtractError("");
    }, );
  });

  test('Should be able to create instance of BeekeeperInstanceHelper', async ({ page }) => {
    await page.evaluate(async (WALLET_OPTIONS) => {
      const provider = await beekeeper();

      new BeekeeperInstanceHelper(provider, WALLET_OPTIONS)
    }, WALLET_OPTIONS);
  });

  test('Should be able to create instance of beekeeper factory', async ({ page }) => {
    await page.evaluate(async () => {
      await factory();
    });
  });

  test.afterAll(async () => {
    await browser.close();
  });
});
