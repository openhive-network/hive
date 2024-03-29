import { test, expect } from '@playwright/test';
import fs from 'fs';

import beekeeperFactory from '../../dist/bundle/node.js';

import { STORAGE_ROOT_NODE } from '../assets/data.js';

const removePossibleBeekeeperWallets = () => {
  if(fs.existsSync(STORAGE_ROOT_NODE))
    fs.rmdirSync(STORAGE_ROOT_NODE, { recursive: true });
};

test.describe('Beekeeper factory tests for Node.js', () => {
  test.beforeEach(removePossibleBeekeeperWallets);
  test.afterEach(removePossibleBeekeeperWallets);

  test('Should be able to init the beekeeper factory', async () => {
    const beekeeper = await beekeeperFactory({ storageRoot: STORAGE_ROOT_NODE });

    beekeeper.createSession("my.salt");

    await beekeeper.delete();
  });

  test('Should be able to get_info based on the created session', async () => {
    const beekeeper = await beekeeperFactory({ storageRoot: STORAGE_ROOT_NODE });

    const session = beekeeper.createSession("my.salt");

    const info = session.getInfo();

    expect(info).toHaveProperty('now');
    expect(info).toHaveProperty('timeout_time');

    const timeMatch = /\d+-[01]\d-[0-3]\dT[0-2]\d:[0-5]\d:[0-5]\d/;

    expect(info.now).toMatch(timeMatch);
    expect(info.timeout_time).toMatch(timeMatch);
  });

  test('Should set a proper timeout', async () => {
    const unlockTimeout = 5;

    const beekeeper = await beekeeperFactory({
      unlockTimeout,
      storageRoot: STORAGE_ROOT_NODE
    });

    const session = beekeeper.createSession("my.salt");

    const info = session.getInfo();

    expect(new Date(info.timeout_time).getTime()).toBe(new Date(info.now).getTime() + (unlockTimeout * 1000 ));
  });

  test('Should be able to create multiple sessions with wallets and delete the beekeeper instance', async () => {
    const beekeeper = await beekeeperFactory({ storageRoot: STORAGE_ROOT_NODE });

    const session1 = beekeeper.createSession("avocado1");
    const session2 = beekeeper.createSession("avocado2");

    await session1.createWallet('w0');
    await session2.createWallet('w1');

    await beekeeper.delete();
  });

  test('Should be able to create a wallet and import and remove keys', async () => {
    const beekeeper = await beekeeperFactory({ storageRoot: STORAGE_ROOT_NODE });

    const session = beekeeper.createSession("my.salt");

    const { wallet: unlocked } = await session.createWallet('w0', 'mypassword');

    await unlocked.importKey('5JkFnXrLM2ap9t3AmAxBJvQHF7xSKtnTrCTginQCkhzU5S7ecPT');
    await unlocked.importKey('5KGKYWMXReJewfj5M29APNMqGEu173DzvHv5TeJAg9SkjUeQV78');

    await unlocked.removeKey('6oR6ckA4TejTWTjatUdbcS98AKETc3rcnQ9dWxmeNiKDzfhBZa');

    const info = unlocked.getPublicKeys();

    expect(info).toStrictEqual(['5RqVBAVNp5ufMCetQtvLGLJo7unX9nyCBMMrTXRWQ9i1Zzzizh']);
  });

  test('Should not be able to import keys after closing a wallet', async () => {
    const beekeeper = await beekeeperFactory({ storageRoot: STORAGE_ROOT_NODE });

    const session = beekeeper.createSession("my.salt");

    const { wallet: unlocked } = await session.createWallet('w0', 'mypassword');

    unlocked.close();

    expect((async() => {
      try {
        await unlocked.importKey('5JkFnXrLM2ap9t3AmAxBJvQHF7xSKtnTrCTginQCkhzU5S7ecPT'); // This should fail

        return false;
      } catch {
        return true;
      }
    })()).toBeTruthy();
  });

  test('Should be able to create multiple wallets and access them using listWallets references', async () => {
    const beekeeper = await beekeeperFactory({ storageRoot: STORAGE_ROOT_NODE });

    const session = beekeeper.createSession("my.salt");

    await session.createWallet('w0', 'mypassword');
    await session.createWallet('w1', 'mypassword');
    await session.createWallet('w2', 'mypassword');

    const info = session.listWallets().map(value => value.name);

    expect(info).toStrictEqual(['w0','w1','w2']);
  });

  test('Shold be able to encrypt and decrypt messages', async () => {
    const input = "Big Brother is Watching You";

    const beekeeper = await beekeeperFactory({ storageRoot: STORAGE_ROOT_NODE });

    const session = beekeeper.createSession("my.salt");

    const { wallet } = await session.createWallet('w0', 'mypassword');

    const inputKey = await wallet.importKey('5KLytoW1AiGSoHHBA73x1AmgZnN16QDgU1SPpG9Vd2dpdiBgSYw');
    const outputKey = await wallet.importKey('5KXNQP5feaaXpp28yRrGaFeNYZT7Vrb1PqLEyo7E3pJiG1veLKG');

    const encrypted = wallet.encryptData(input, inputKey, outputKey);

    const retVal = wallet.decryptData(encrypted, inputKey, outputKey);

    expect(retVal).toBe(input);
  });
});
