import { ChromiumBrowser, ConsoleMessage, Page, chromium } from 'playwright';
import { expect, test } from '@playwright/test';

import { STORAGE_ROOT, WALLET_OPTIONS } from '../assets/data';

let browser: ChromiumBrowser;
let page1: Page, page2: Page, page3: Page;

const handlePageLoad = async(page: Page) => {
  page.on('console', (msg: ConsoleMessage) => {
    console.log('>>', msg.type(), msg.text())
  });

  await page.goto(`http://localhost:8080/__tests__/assets/test.html`);
  await page.waitForURL('**/test.html', { waitUntil: 'load' });
}

const saveKeys = (page: Page, options: { walletDir: string, args: string[], close: boolean }) =>
  page.evaluate(async({ walletDir, args, close }) => {
    const provider = await beekeeper();
    const fs = provider.FS;
    fs.mkdir(walletDir);
    fs.mount(fs.filesystems.IDBFS, {}, walletDir);

    const sync = (initFs = false) => new Promise((resolve, reject) => {
      fs.syncfs(initFs, (err?: unknown) => {
        if (err) reject(err);

        resolve(undefined);
      });
    });

    await sync(true);

    const helper = BeekeeperInstanceHelper.for(provider);

    const api = new helper(args);

    api.create_with_password(api.implicitSessionToken, "w0", "badf00d");
    api.importKey(api.implicitSessionToken, "w0", "5JkFnXrLM2ap9t3AmAxBJvQHF7xSKtnTrCTginQCkhzU5S7ecPT");

    await sync();

    if(close) {
      api.close(api.implicitSessionToken, "w0");
      api.closeSession(api.implicitSessionToken);
      api.deleteInstance();
    }

    await sync();

    return fs.readdir(walletDir);
  }, options);

const retrieveKeys = (page: Page, options: { walletDir: string, args: string[], close: boolean }) =>
  page.evaluate(async({ walletDir, args, close }) => {
    const provider = await beekeeper();
    const fs = provider.FS;
    fs.mkdir(walletDir);
    fs.mount(fs.filesystems.IDBFS, {}, walletDir);

    const sync = (initFs = false) => new Promise((resolve, reject) => {
      fs.syncfs(initFs, (err?: unknown) => {
        if (err) reject(err);

        resolve(undefined);
      });
    });

    await sync(true);

    const helper = BeekeeperInstanceHelper.for(provider);

    const api = new helper(args);

    api.open(api.implicitSessionToken, "w0");
    api.unlock(api.implicitSessionToken, "w0", "badf00d");

    const keys = api.getPublicKeys(api.implicitSessionToken);

    if(close) {
      api.close(api.implicitSessionToken, "w0");
      api.closeSession(api.implicitSessionToken);
      api.deleteInstance();
    }

    return keys;
  }, options);

test.describe('WASM storage tests', () => {
  test.beforeAll(async () => {
    browser = await chromium.launch({
      headless: true
    });

    const context1 = await browser.newContext();
    const context2 = await browser.newContext();

    page1 = await context1.newPage();
    page2 = await context1.newPage();
    page3 = await context2.newPage();
    await handlePageLoad(page1);
    await handlePageLoad(page2);
    await handlePageLoad(page3);
  });

  test.beforeEach(async({ page }) => {
    await handlePageLoad(page);
  });

  test('Should have filesystem in the provider', async ({ page }) => {
    const fsType = await page.evaluate(async () => {
      const provider = await beekeeper();
      return typeof provider.FS;
    });

    expect(fsType).toBe('object');
  });

  test('Should be able to mount and sync the filesystem', async ({ page }) => {
    await page.evaluate(async (walletDir) => {
      const provider = await beekeeper();
      const fs = provider.FS;
      fs.mkdir(walletDir);
      fs.mount(fs.filesystems.IDBFS, {}, walletDir);

      await new Promise((resolve, reject) => {
        fs.syncfs(true, (err?: unknown) => {
          if (err) reject(err);

          resolve(undefined);
        });
      });
    }, STORAGE_ROOT);
  });

  test('Should be able to create a directory and make it available in the same session', async () => {
    const dir = await page1.evaluate(async (walletDir) => {
      const provider = await beekeeper();
      const fs = provider.FS;
      fs.mkdir(walletDir);
      fs.mount(fs.filesystems.IDBFS, {}, walletDir);

      const sync = (init = false) => new Promise((resolve, reject) => {
        fs.syncfs(init, (err?: unknown) => {
          if (err) reject(err);

          resolve(undefined);
        });
      });

      await sync(true);

      fs.writeFile(`${walletDir}/beekeeper`, 'beekeeper');

      await sync();

      return fs.readdir(walletDir);
    }, STORAGE_ROOT);

    expect(dir).toStrictEqual([ '.', '..', 'beekeeper' ]);
  });

  test('Should not contain any data from the previous test in a new context', async ({ page }) => {
    const dir = await page.evaluate(async (walletDir) => {
      const provider = await beekeeper();
      const fs = provider.FS;
      fs.mkdir(walletDir);
      fs.mount(fs.filesystems.IDBFS, {}, walletDir);

      await new Promise((resolve, reject) => {
        fs.syncfs(true, (err?: unknown) => {
          if (err) reject(err);

          resolve(undefined);
        });
      });

      return fs.readdir(walletDir);
    }, STORAGE_ROOT);

    expect(dir).toStrictEqual([ '.', '..' ]);
  });

  test('Should contain data from the previous test in the same context', async () => {
    const dir = await page2.evaluate(async (walletDir) => {
      const provider = await beekeeper();
      const fs = provider.FS;
      fs.mkdir(walletDir);
      fs.mount(fs.filesystems.IDBFS, {}, walletDir);

      await new Promise((resolve, reject) => {
        fs.syncfs(true, (err?: unknown) => {
          if (err) reject(err);

          resolve(undefined);
        });
      });

      return fs.readdir(walletDir);
    }, STORAGE_ROOT);

    expect(dir).toStrictEqual([ '.', '..', 'beekeeper' ]);
  });

  test('Should be able to init beekeeper and save the wallet file with explicitly closing the instance of beekeeper', async () => {
    const dir = await saveKeys(page1, { close: true, walletDir: STORAGE_ROOT, args: WALLET_OPTIONS });

    expect(dir).toStrictEqual([ '.', '..', 'beekeeper', 'directory with spaces' ]);
  });

  test('Should be able to init beekeeper and save the wallet file without explicitly closing the instance of beekeeper', async () => {
    const dir = await saveKeys(page3, { close: false, walletDir: STORAGE_ROOT, args: WALLET_OPTIONS });
    expect(dir).toStrictEqual([ '.', '..', 'directory with spaces' ]);
  });

  test('Should not be able to list the previously imported key from other context', async ({ page }) => {
    const keys = await page.evaluate(async ({ walletDir, args }) => {
      const provider = await beekeeper();
      const fs = provider.FS;
      fs.mkdir(walletDir);
      fs.mount(fs.filesystems.IDBFS, {}, walletDir);

      const sync = (initFs = false) => new Promise((resolve, reject) => {
        fs.syncfs(initFs, (err?: unknown) => {
          if (err) reject(err);

          resolve(undefined);
        });
      });

      await sync(true);

      const helper = BeekeeperInstanceHelper.for(provider);

      const api = new helper(args);

      return api.listWallets(api.implicitSessionToken);
    }, { walletDir: STORAGE_ROOT, args: WALLET_OPTIONS });

    expect(keys).toStrictEqual({"wallets": []});
  });

  test('Should be able to list the previously imported key from another page with the same browser context with explicitly closing the instance of beekeeper', async () => {
    const keys = await retrieveKeys(page2, { close: true, walletDir: STORAGE_ROOT, args: WALLET_OPTIONS });

    expect(keys).toStrictEqual({"keys": [{"public_key": "STM5RqVBAVNp5ufMCetQtvLGLJo7unX9nyCBMMrTXRWQ9i1Zzzizh"}]});
  });

  test('Should be able to list the previously imported key without explicitly closing the instance of beekeeper', async () => {
    const keys = await retrieveKeys(page3, { close: false, walletDir: STORAGE_ROOT, args: WALLET_OPTIONS });

    expect(keys).toStrictEqual({"keys": [{"public_key": "STM5RqVBAVNp5ufMCetQtvLGLJo7unX9nyCBMMrTXRWQ9i1Zzzizh"}]});
  });

  test.afterAll(async () => {
    await browser.close();
  });
});
