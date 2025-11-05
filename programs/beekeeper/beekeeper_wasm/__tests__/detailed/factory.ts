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

  test('Should be able to display only private keys in specific wallet', async ({ beekeeperTest }) => {
    const retVal = await beekeeperTest(async ({ beekeeper }) => {
      const session = beekeeper.createSession("my.salt");

      const { wallet: unlocked1 } = await session.createWallet('w0', 'mypassword');
      const { wallet: unlocked2 } = await session.createWallet('w1', 'mypassword');

      await unlocked1.importKey('5JkFnXrLM2ap9t3AmAxBJvQHF7xSKtnTrCTginQCkhzU5S7ecPT');
      await unlocked2.importKey('5KGKYWMXReJewfj5M29APNMqGEu173DzvHv5TeJAg9SkjUeQV78');

      return unlocked1.getPublicKeys();
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

  test('Should be able to create a wallet, import keys and check if matching key exists', async ({ beekeeperTest }) => {
    const retVal = await beekeeperTest(async ({ beekeeper }) => {
      const session = beekeeper.createSession("my.salt");

      const { wallet: unlocked } = await session.createWallet('w0', 'mypassword');

      await unlocked.importKey('5JkFnXrLM2ap9t3AmAxBJvQHF7xSKtnTrCTginQCkhzU5S7ecPT');

      return unlocked.hasMatchingPrivateKey('STM5RqVBAVNp5ufMCetQtvLGLJo7unX9nyCBMMrTXRWQ9i1Zzzizh');
    });

    expect(retVal).toBeTruthy();
  });

  test('Should wallet be a temporary wallet', async ({ beekeeperTest }) => {
    const retVal = await beekeeperTest(async ({ beekeeper }) => {
      const session = beekeeper.createSession("my.salt");

      const { wallet: unlocked } = await session.createWallet('w0', 'mypassword', true);

      return unlocked.isTemporary;
    });

    expect(retVal).toBeTruthy();
  });

  test('Should automatically deduce wallet type based on the inMemory setting', async ({ beekeeperTest }) => {
    const retVal = await beekeeperTest(async ({ provider, storageRoot }) => {
      const bkInFs = await provider.default({ storageRoot, inMemory: false, enableLogs: false });
      const bkInMemory = await provider.default({ storageRoot, inMemory: true, enableLogs: false });

      const sessionFs = bkInFs.createSession("my.salt");
      const sessionInMemory = bkInMemory.createSession("my.salt");

      const { wallet: unlockedInMemory } = await sessionInMemory.createWallet('w0_inmemory');
      const { wallet: unlockedFs } = await sessionFs.createWallet('w0_infs');

      return [unlockedInMemory.isTemporary, unlockedFs.isTemporary];
    });

    expect(retVal[0]).toBeTruthy();
    expect(retVal[1]).toBeFalsy();
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

  test('Should be able to encrypt and decrypt messages', async ({ beekeeperTest }) => {
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

  test('Should be able to sign digest', async ({ beekeeperTest }) => {
    const retVal = await beekeeperTest(async ({ beekeeper }) => {
      const digestStr = "390f34297cfcb8fa4b37353431ecbab05b8dc0c9c15fb9ca1a3d510c52177542";
      // Convert hex string to Uint8Array
      const uint8Array = new Uint8Array(digestStr.match(/.{1,2}/g)!.map(byte => parseInt(byte, 16)));

      const session = beekeeper.createSession("my.salt");

      const { wallet } = await session.createWallet('w0', 'mypassword');

      const publicKey = await wallet.importKey('5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n');

      const signatureStr = wallet.signDigest(publicKey, digestStr);
      const signatureHex = wallet.signDigest(publicKey, uint8Array);

      return {
        fromString: signatureStr,
        fromHex: signatureHex
      };
    });

    const expected = "1f17cc07f7c769073d39fac3385220b549e261fb33c5f619c5dced7f5b0fe9c0954f2684e703710840b7ea01ad7238b8db1d8a9309d03e93de212f86de38d66f21";

    expect(retVal).toStrictEqual({
      fromString: expected,
      fromHex: expected
    });
  });

  test.afterAll(async () => {
    await browser.close();
  });
});
