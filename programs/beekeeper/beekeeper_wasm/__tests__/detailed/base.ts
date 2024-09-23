import { chromium, ChromiumBrowser, expect } from '@playwright/test';
import { test } from '../assets/jest-helper.js'
import { WALLET_OPTIONS_NODE } from '../assets/data.js';

let browser!: ChromiumBrowser;

test.describe('WASM Base tests', () => {
  test.beforeAll(async () => {
    browser = await chromium.launch({
      headless: true
    });
  });

  test('Should be able to create instance of StringList', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ provider }) => {
      return new provider.StringList();
    });

    expect(retVal).toBeDefined();
  });

  test('Should be able to create instance of beekeeper_api', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ provider }, WALLET_OPTIONS_NODE) => {
      const beekeeperOptions = new provider.StringList();

      WALLET_OPTIONS_NODE.forEach((opt) => void beekeeperOptions.push_back(opt));

      return new provider.beekeeper_api(beekeeperOptions);
    }, WALLET_OPTIONS_NODE);

    expect(retVal).toBeDefined();
  });

  test('Should be able to create instance of ExtractError - import script', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ ExtractError }) => {
      return new ExtractError({ a: 10 }).parsed;
    });

    expect(retVal).toStrictEqual({ a: 10 });
  });

  test('Should be able to create instance of BeekeeperInstanceHelper', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE) => {
      return new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);
    }, WALLET_OPTIONS_NODE);

    expect(retVal).toBeDefined();
  });

  test.afterAll(async () => {
    await browser.close();
  });
});
