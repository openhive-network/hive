import { chromium, ChromiumBrowser, expect } from '@playwright/test';
import { test } from '../assets/jest-helper.js';

let browser!: ChromiumBrowser;

test.describe('Beekeeper factory tests for Node.js', () => {
  test.beforeAll(async () => {
    browser = await chromium.launch({
      headless: true
    });
  });

  test('Should be able to retrieve package version', async ({ beekeeperTest }) => {
    const version = await beekeeperTest(({ beekeeper }) => {
      return beekeeper.getVersion();
    });

    expect(typeof version).toBe('string');
  });

  test('Should be able to init the beekeeper factory', async ({ beekeeperTest }) => {
    await beekeeperTest(async ({ beekeeper }) => {
      beekeeper.createSession("my.salt");

      await beekeeper.delete();
    });
  });

  test('Should be able to get_info based on the created session', async ({ beekeeperTest }) => {
    const retVal = await beekeeperTest.dynamic(async ({ beekeeper }) => {
      const session = beekeeper.createSession("my.salt");

      return session.getInfo();
    });

    expect(retVal).toHaveProperty('now');
    expect(retVal).toHaveProperty('timeoutTime');

    expect(retVal.now).toBeInstanceOf(Date);
    expect(retVal.timeoutTime).toBeInstanceOf(Date);
  });

  test('Should set a proper timeout', async ({ beekeeperTest }) => {
    const retVal = await beekeeperTest(async ({ provider }) => {
      const beekeeper = await provider.default({ unlockTimeout: 5, enableLogs: false });

      const session = beekeeper.createSession("my.salt");

      const sesInfo = session.getInfo();

      const data = {
        actual: sesInfo.timeoutTime.getTime(),
        expected: sesInfo.now.getTime() + (5 * 1000)
      };

      return data.actual === data.expected;
    });

    expect(retVal).toBeTruthy();
  });

  test('Should be able to create multiple sessions with wallets and delete the beekeeper instance', async ({ beekeeperTest }) => {
    await expect(beekeeperTest(async ({ beekeeper }) => {
      const session1 = beekeeper.createSession("avocado1");
      const session2 = beekeeper.createSession("avocado2");

      await session1.createWallet('w0');
      await session2.createWallet('w1');

      await beekeeper.delete();
    })).resolves.toBeUndefined();
  });

  test('Should be able to create a wallet and import and remove keys', async ({ beekeeperTest }) => {
    const retVal = await beekeeperTest(async ({ beekeeper }) => {
      const session = beekeeper.createSession("my.salt");

      const { wallet: unlocked } = await session.createWallet('w0', 'mypassword');

      await unlocked.importKey('5JkFnXrLM2ap9t3AmAxBJvQHF7xSKtnTrCTginQCkhzU5S7ecPT');
      await unlocked.importKey('5KGKYWMXReJewfj5M29APNMqGEu173DzvHv5TeJAg9SkjUeQV78');

      await unlocked.removeKey('STM6oR6ckA4TejTWTjatUdbcS98AKETc3rcnQ9dWxmeNiKDzfhBZa');

      return unlocked.getPublicKeys();
    });

    expect(retVal).toStrictEqual(['STM5RqVBAVNp5ufMCetQtvLGLJo7unX9nyCBMMrTXRWQ9i1Zzzizh']);
  });

  test('Should be able to create a temporary wallet and import and remove keys', async ({ beekeeperTest }) => {
    const retVal = await beekeeperTest(async ({ beekeeper }) => {
      const session = beekeeper.createSession("my.salt");

      const { wallet: unlocked } = await session.createWallet('w0', 'mypassword', true);

      await unlocked.importKey('5JkFnXrLM2ap9t3AmAxBJvQHF7xSKtnTrCTginQCkhzU5S7ecPT');
      await unlocked.importKey('5KGKYWMXReJewfj5M29APNMqGEu173DzvHv5TeJAg9SkjUeQV78');

      await unlocked.removeKey('STM6oR6ckA4TejTWTjatUdbcS98AKETc3rcnQ9dWxmeNiKDzfhBZa');

      return unlocked.getPublicKeys();
    });

    expect(retVal).toStrictEqual(['STM5RqVBAVNp5ufMCetQtvLGLJo7unX9nyCBMMrTXRWQ9i1Zzzizh']);
  });

  test('Should wallet be a temporary wallet', async ({ beekeeperTest }) => {
    const retVal = await beekeeperTest(async ({ beekeeper }) => {
      const session = beekeeper.createSession("my.salt");

      const { wallet: unlocked } = await session.createWallet('w0', 'mypassword', true);

      return unlocked.isTemporary;
    });

    expect(retVal).toBeTruthy();
  });

  test('Should have a temporary wallet', async ({ beekeeperTest }) => {
    const retVal = await beekeeperTest(async ({ beekeeper }) => {
      const session = beekeeper.createSession("my.salt");

      await session.createWallet('w0', 'mypassword', true);

      return session.hasWallet('w0');
    });

    expect(retVal).toBeTruthy();
  });

  test('Should not be able to import keys after closing a wallet', async ({ beekeeperTest }) => {
    await expect(beekeeperTest(async ({ beekeeper }) => {
      const session = beekeeper.createSession("my.salt");

      const { wallet: unlocked } = await session.createWallet('w0', 'mypassword');

      unlocked.close();

      await unlocked.importKey('5JkFnXrLM2ap9t3AmAxBJvQHF7xSKtnTrCTginQCkhzU5S7ecPT'); // This should fail
    })).rejects.toThrow(/Wallet not found: w0/);
  });

  test('Should be able to create multiple wallets and access them using listWallets references', async ({ beekeeperTest }) => {
    const retVal = await beekeeperTest(async ({ beekeeper }) => {
      const session = beekeeper.createSession("my.salt");

      await session.createWallet('w0', 'mypassword');
      await session.createWallet('w1', 'mypassword');
      await session.createWallet('w2', 'mypassword');

      return session.listWallets().map(value => value.name);
    });

    expect(retVal).toStrictEqual(['w0','w1','w2']);
  });

  test('Shold be able to encrypt and decrypt messages', async ({ beekeeperTest }) => {
    const retVal = await beekeeperTest(async ({ beekeeper }) => {
      const input = "Big Brother is Watching You";

      const session = beekeeper.createSession("my.salt");

      const { wallet } = await session.createWallet('w0', 'mypassword');

      const inputKey = await wallet.importKey('5KLytoW1AiGSoHHBA73x1AmgZnN16QDgU1SPpG9Vd2dpdiBgSYw');
      const outputKey = await wallet.importKey('5KXNQP5feaaXpp28yRrGaFeNYZT7Vrb1PqLEyo7E3pJiG1veLKG');

      const encrypted = wallet.encryptData(input, inputKey, outputKey);

      return wallet.decryptData(encrypted, inputKey, outputKey);
    });

    expect(retVal).toBe("Big Brother is Watching You");
  });

  test.afterAll(async () => {
    await browser.close();
  });
});
