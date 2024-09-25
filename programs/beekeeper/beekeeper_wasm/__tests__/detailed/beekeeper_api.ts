import { chromium, ChromiumBrowser, expect } from '@playwright/test';
import { test } from '../assets/jest-helper.js'

import { WALLET_OPTIONS_NODE } from '../assets/data.js';

const walletNames = ["w0","w1","w2","w3","w4","w5","w6","w7","w8","w9","w10"];

const keys =
[
  ['5JkFnXrLM2ap9t3AmAxBJvQHF7xSKtnTrCTginQCkhzU5S7ecPT', 'STM5RqVBAVNp5ufMCetQtvLGLJo7unX9nyCBMMrTXRWQ9i1Zzzizh'],
  ['5KGKYWMXReJewfj5M29APNMqGEu173DzvHv5TeJAg9SkjUeQV78', 'STM6oR6ckA4TejTWTjatUdbcS98AKETc3rcnQ9dWxmeNiKDzfhBZa'],
  ['5KNbAE7pLwsLbPUkz6kboVpTR24CycqSNHDG95Y8nbQqSqd6tgS', 'STM7j1orEPpWp4bU2SuH46eYXuXkFKEMeJkuXkZVJSaru2zFDGaEH'],
  ['5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n', 'STM6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4'],
  ['5J8C7BMfvMFXFkvPhHNk2NHGk4zy3jF4Mrpf5k5EzAecuuzqDnn', 'STM6Pg5jd1w8rXgGoqvpZXy1tHPdz43itPW6L2AGJuw8kgSAbtsxm'],
  ['5J15npVK6qABGsbdsLnJdaF5esrEWxeejeE3KUx6r534ug4tyze', 'STM6TqSJaS1aRj6p6yZEo5xicX7bvLhrfdVqi5ToNrKxHU3FRBEdW'],
  ['5K1gv5rEtHiACVTFq9ikhEijezMh4rkbbTPqu4CAGMnXcTLC1su', 'STM8LbCRyqtXk5VKbdFwK1YBgiafqprAd7yysN49PnDwAsyoMqQME'],
  ['5KLytoW1AiGSoHHBA73x1AmgZnN16QDgU1SPpG9Vd2dpdiBgSYw', 'STM8FDsHdPkHbY8fuUkVLyAmrnKMvj6DddLopi3YJ51dVqsG9vZa4'],
  ['5KXNQP5feaaXpp28yRrGaFeNYZT7Vrb1PqLEyo7E3pJiG1veLKG', 'STM6a34GANY5LD8deYvvfySSWGd7sPahgVNYoFPapngMUD27pWb45'],
  ['5KKvoNaCPtN9vUEU1Zq9epSAVsEPEtocbJsp7pjZndt9Rn4dNRg', 'STM8mmxXz5BfQc2NJfqhiPkbgcyJm4EvWEr2UAUdr56gEWSN9ZnA5'],
  ['5Jen4tBsMEDyr4iNfDUTiUXbdrZZ7BuFa5oVHxBT7Zybm71Fmjz', 'STM7FMWqA7f5oYov4pmhfUDd4JJENRguZ4Sv7d3i2jt6Rz2Sg37fh'],
  ['5Jo6ALLFrTBK9pbzdNYKxZt6wPfKtRa9bXrek7Ypo988PKJZWgV', 'STM6iEHkB8ohBUCGWfftcEWmNyRtqZhu4m8sbc5c2QYv2AWuMHt5k'],
  ['5Js14qmTQ8Pf6mBzBaagKQ5Tc8tqgeLqncnKJoL6dEqYzzT7Mvf', 'STM6SKxp2eB7Zc4bFGVwQPNijyWouNidPWvyFsSALvavQWjRhqJXf']
];

const signData =
[
  {
    'public_key': keys[3][1],
    'sig_digest': '390f34297cfcb8fa4b37353431ecbab05b8dc0c9c15fb9ca1a3d510c52177542',
    'binary_transaction_body': '000000000000000000000000',
    'transaction_body': '{}',
    'expected_signature': '1f17cc07f7c769073d39fac3385220b549e261fb33c5f619c5dced7f5b0fe9c0954f2684e703710840b7ea01ad7238b8db1d8a9309d03e93de212f86de38d66f21'
  },
  {
    'public_key': keys[3][1],
    'sig_digest': '614e645c13b351b56d9742b358e3c3da58fa1a6a0036a01d3163c21aa2c8a99c',
    'binary_transaction_body': '5f00c58fb5f9854fb664010209696e69746d696e657205616c6963659a020000000000002320bcbe056d656d6d6d00',
    'transaction_body': '{"ref_block_num":95,"ref_block_prefix":4189425605,"expiration":"2023-07-18T08:38:29","operations":[{"type":"transfer_operation","value":{"from":"initminer","to":"alice","amount":{"amount":"666","precision":3,"nai":"@@000000021"},"memo":"memmm"}}],"extensions":[],"signatures":[],"transaction_id":"cc9630cdbc39da1c9b6264df3588c7bedb5762fa","block_num":0,"transaction_num":0}',
    'expected_signature': '1f69e091fc79b0e8d1812fc662f12076561f9e38ffc212b901ae90fe559f863ad266fe459a8e946cff9bbe7e56ce253bbfab0cccdde944edc1d05161c61ae86340'
  },
];

let browser!: ChromiumBrowser;

test.describe('WASM beekeeper_api tests for Node.js', () => {
  test.beforeAll(async () => {
    browser = await chromium.launch({
      headless: true
    });
  });

  test('Should be able to get sign digest', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, signData) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      const data = signData[0];

      const session = api.createSession('pear');

      api.create_with_password(session, 'w3', 'pass');

      const key = api.importKey(session, 'w3', '5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n');

      const signDigest = api.signDigest(session, data.sig_digest, key);

      return {
        signDigest,
        expected: data.expected_signature
      }
    }, WALLET_OPTIONS_NODE, signData);

    expect(retVal.signDigest).toBe(retVal.expected);
  });

  test('Should be able to list wallets', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      const session = api.createSession('pear');

      api.create_with_password(session, 'w3', 'pass');
      api.create_with_password(session, 'w4', 'pass');
      api.create_with_password(session, 'w5', 'pass');

      return api.listWallets(session).wallets;
    }, WALLET_OPTIONS_NODE);

    expect(retVal).toStrictEqual([
      { name: 'w3', unlocked: true },
      { name: 'w4', unlocked: true },
      { name: 'w5', unlocked: true }
    ]);
  });

  test('Should require keys in wallet', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      const session = api.createSession('pear');

      api.create_with_password(session, 'w3', 'pass');

      api.importKey(session, 'w3', '5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n');

      const publicKeys = api.getPublicKeys(session);

      return publicKeys.keys;
    }, WALLET_OPTIONS_NODE);

    expect(retVal).toStrictEqual([
      { public_key: "STM6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4" }
    ]);
    expect(retVal[0].public_key).toBe('STM6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4');
  });

  test('Should be able to find a key', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, keys) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      const session = api.createSession('pear');

      api.create_with_password(session, 'w3', 'pass');

      api.importKey(session, 'w3', keys[3][0]);

      const publicKeys = api.getPublicKeys(session);

      const indexes = [1, 2, 3, 10, 11, 12];

      for (let i = 0; i < indexes.length; ++i)
        if (publicKeys.keys.find(value => value.public_key === keys[indexes[i]][1]))
          return true;

      return false;
    }, WALLET_OPTIONS_NODE, keys);

    expect(retVal).toBeTruthy();
  });

  test('Should create a wallet with password', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      api.create_with_password(api.implicitSessionToken, 'w0', 'this_is_password');

      return api.hasWallet(api.implicitSessionToken, 'w0');
    }, WALLET_OPTIONS_NODE);

    expect(retVal).toBeTruthy();
  });

  test('Should unlock a wallet', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      const session = api.createSession('pear');

      api.create_with_password(session, 'w0', 'pass');

      return api.listWallets(session).wallets[0].unlocked;
    }, WALLET_OPTIONS_NODE);

    expect(retVal).toBeTruthy();
  });

  test('Should be able to import keys', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, keys, walletNames) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      const session = api.createSession('pear');

      api.create_with_password(session, walletNames[0], 'pass');

      api.importKey(session, walletNames[0], keys[3][0]);
      api.importKey(session, walletNames[0], keys[4][0]);

      const publicKeys = api.getPublicKeys(session);

      return publicKeys.keys;
    }, WALLET_OPTIONS_NODE, keys, walletNames);

    expect(retVal).toStrictEqual([
      { public_key: "STM6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4" },
      { public_key: "STM6Pg5jd1w8rXgGoqvpZXy1tHPdz43itPW6L2AGJuw8kgSAbtsxm" }
    ]);
  });

  test('Should be able to close a wallet', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, walletNames) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      const session = api.createSession('pear');

      api.create_with_password(session, walletNames[0], 'pass');

      api.close(session, walletNames[0]);

      const wallets = api.listWallets(session);

      return wallets.wallets;
    }, WALLET_OPTIONS_NODE, walletNames);

    expect(retVal).not.toContain('w0');
  });

  test('Should be able to create a wallet', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, walletNames) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      api.create(api.implicitSessionToken, walletNames[0]);

      const wallets = api.listWallets(api.implicitSessionToken);

      return wallets.wallets[0].name;
    }, WALLET_OPTIONS_NODE, walletNames);

    expect(retVal).toEqual('w0');
  });

  test('Should be able to close a session', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      api.createSession(api.implicitSessionToken);

      try {
        api.closeSession(api.implicitSessionToken);
        return true;
      } catch {
        return false;
      }
    }, WALLET_OPTIONS_NODE);

    expect(retVal).toBeTruthy();
  });

  test('Should be able to create a few wallets', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, walletNames) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      const session = api.createSession('pear');

      api.create_with_password(session, walletNames[1], 'cherry');
      api.create(session, walletNames[2]);
      api.create(session, walletNames[3]);

      return {
        w0: api.listWallets(session).wallets[0],
        w1: api.listWallets(session).wallets[1],
        w2: api.listWallets(session).wallets[2]
      };
    }, WALLET_OPTIONS_NODE, walletNames);

    expect(retVal.w0.name).toEqual('w1');
    expect(retVal.w1.name).toEqual('w2');
    expect(retVal.w2.name).toEqual('w3');
  });

  test('Should be able to lock a wallet', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, walletNames) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      const session = api.createSession('pear');

      api.create_with_password(session, walletNames[1], 'cherry');
      api.lock(session, walletNames[1]);

      return api.listWallets(session).wallets[0];
    }, WALLET_OPTIONS_NODE, walletNames);

    expect(retVal.unlocked).toBeFalsy();
  });

  test('Should be able to lock only one wallet from many', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, walletNames) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      const session = api.createSession('pear');

      api.create_with_password(session, walletNames[1], 'cherry');

      api.create_with_password(session, walletNames[2], 'pass');

      api.lock(session, walletNames[2]);

      return {
        w0: api.listWallets(session).wallets[0],
        w1: api.listWallets(session).wallets[1]
      }
    }, WALLET_OPTIONS_NODE, walletNames);

    expect(retVal.w0.unlocked).toBeTruthy();
    expect(retVal.w1.unlocked).toBeFalsy();
  });

  test('Should be able to lock and unlock a wallet', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, walletNames) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      const session = api.createSession('pear');

      api.create_with_password(session, walletNames[1], 'cherry');

      api.lock(session, walletNames[1]);
      const locked = api.listWallets(session).wallets[0].unlocked;

      api.unlock(session, walletNames[1], 'cherry');
      const unlocked = api.listWallets(session).wallets[0].unlocked;

      return {
        locked,
        unlocked
      };
    }, WALLET_OPTIONS_NODE, walletNames);

    expect(retVal.locked).toBeFalsy();
    expect(retVal.unlocked).toBeTruthy();
  });

  test('Should be able to lock all wallets', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, walletNames) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      const session = api.createSession('pear');

      api.create_with_password(session, walletNames[1], 'cherry');

      api.create_with_password(session, walletNames[2], 'pass');

      api.lockAll(session);

      return {
        w0: api.listWallets(session).wallets[0],
        w1: api.listWallets(session).wallets[1]
      }
    }, WALLET_OPTIONS_NODE, walletNames);

    expect(retVal.w0.unlocked).toBeFalsy();
    expect(retVal.w1.unlocked).toBeFalsy();
  });

  test('Should be able to close only one wallet from many', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, walletNames) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      const session = api.createSession('pear');

      api.create_with_password(session, walletNames[1], 'cherry');
      api.create_with_password(session, walletNames[2], 'pass');
      api.create_with_password(session, walletNames[3], 'pass');

      api.close(session, walletNames[2]);

      return api.listWallets(session).wallets;
    }, WALLET_OPTIONS_NODE, walletNames);

    expect(retVal).toEqual([
      { name: 'w1', unlocked: true },
      { name: 'w2', unlocked: false },
      { name: 'w3', unlocked: true }
    ]);
  });

  test('Should not be able to open a wallet without the password', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, walletNames) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      const session = api.createSession('pear');

      api.create(session, walletNames[1]);

      api.open(session, walletNames[1]);

      return api.listWallets(session).wallets[0].unlocked;
    }, WALLET_OPTIONS_NODE, walletNames);

    expect(retVal).toBeFalsy();
  });

  test('Should be able to remove a key', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, keys, walletNames) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      const session = api.createSession('pear');

      api.create_with_password(session, walletNames[1], 'cherry');
      api.importKey(session, walletNames[1], keys[1][0]);

      api.removeKey(session, walletNames[1], keys[1][1]);

      return api.getPublicKeys(session).keys;
    }, WALLET_OPTIONS_NODE, keys, walletNames);

    expect(retVal).toStrictEqual([]);
  });

  test('should be able to import a few keys at once from a string list', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, keys, walletNames) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      const session = api.createSession('pear');

      const setOfKeys = new provider.StringList();

      setOfKeys.push_back(keys[10][0]);
      setOfKeys.push_back(keys[11][0]);
      setOfKeys.push_back(keys[12][0]);


      api.create_with_password(session, walletNames[1], 'cherry');
      api.importKeys(session, walletNames[1], setOfKeys);

      return api.getPublicKeys(session).keys;
    }, WALLET_OPTIONS_NODE, keys, walletNames);

    expect(retVal).toEqual([
      { public_key: 'STM6SKxp2eB7Zc4bFGVwQPNijyWouNidPWvyFsSALvavQWjRhqJXf' },
      { public_key: 'STM6iEHkB8ohBUCGWfftcEWmNyRtqZhu4m8sbc5c2QYv2AWuMHt5k' },
      { public_key: 'STM7FMWqA7f5oYov4pmhfUDd4JJENRguZ4Sv7d3i2jt6Rz2Sg37fh' }
    ]);
  });

  test('Should be able to delete an api instance', async ({ beekeeperWasmTest }) => {
    await expect(beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      api.deleteInstance();
    }, WALLET_OPTIONS_NODE)).resolves.toBeUndefined()
  });

  test('Should be able to get session info', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      const session = api.createSession('pear');

      const info = api.getInfo(session);

      const now = new Date(info.now);
      const timeoutTime = new Date(info.timeout_time);

      const nowCheck = Number.isNaN(now.getTime());
      const timeoutTimeCheck = Number.isNaN(timeoutTime.getTime());

      return {
        nowCheck,
        timeoutTimeCheck
      };
    }, WALLET_OPTIONS_NODE);

    expect(retVal.nowCheck).toBeFalsy();
    expect(retVal.timeoutTimeCheck).toBeFalsy();
  });

  test('Create 3 wallets and add to every wallet 3 the same keys. Every key should be displayed only once', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, walletNames, keys) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      api.create(api.implicitSessionToken, walletNames[4]);
      api.create(api.implicitSessionToken, walletNames[5]);
      api.create(api.implicitSessionToken, walletNames[6]);

      // Import keys 7-9 to wallets 4-6
      for(let walletNo = 4; walletNo <= 6; ++walletNo)
        for(let keyNo = 7; keyNo <= 9; ++keyNo)
          api.importKey(api.implicitSessionToken, walletNames[walletNo], keys[keyNo][0]);

      return api.getPublicKeys(api.implicitSessionToken).keys;
    }, WALLET_OPTIONS_NODE, walletNames, keys);

    expect(retVal).toStrictEqual([
      { public_key: 'STM6a34GANY5LD8deYvvfySSWGd7sPahgVNYoFPapngMUD27pWb45' },
      { public_key: 'STM8FDsHdPkHbY8fuUkVLyAmrnKMvj6DddLopi3YJ51dVqsG9vZa4' },
      { public_key: 'STM8mmxXz5BfQc2NJfqhiPkbgcyJm4EvWEr2UAUdr56gEWSN9ZnA5' }
    ]);
  });

  test('Remove all keys from 3 wallets. As a result all keys are removed in mentioned wallets', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, walletNames, keys) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      api.create(api.implicitSessionToken, walletNames[4]);
      api.create(api.implicitSessionToken, walletNames[5]);
      api.create(api.implicitSessionToken, walletNames[6]);

      // Import keys 7-9 to wallets 4-6
      for(let walletNo = 4; walletNo <= 6; ++walletNo)
        for(let keyNo = 7; keyNo <= 9; ++keyNo)
          api.importKey(api.implicitSessionToken, walletNames[walletNo], keys[keyNo][0]);

      // Remove keys 7-9 to wallets 4-6
      for(let walletNo = 4; walletNo <= 6; ++walletNo)
        for(let keyNo = 7; keyNo <= 9; ++keyNo)
          api.removeKey(api.implicitSessionToken!, walletNames[walletNo], keys[keyNo][1]);

      return api.getPublicKeys(api.implicitSessionToken).keys;
    }, WALLET_OPTIONS_NODE, walletNames, keys);

    expect(retVal).toStrictEqual([]);
  });

  test('Close implicitly created session. Create 3 new sessions. Every session has a 1 wallet. Every wallet has unique 1 key. As a result there are 3 keys', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, walletNames, keys) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      const publicKeys: { public_key: string }[][] = [];

      api.createSession(api.implicitSessionToken);

      api.closeSession(api.implicitSessionToken);

      const sessions = [
        { keyNo: 4, session: api.createSession('avocado') },
        { keyNo: 5, session: api.createSession('avocado') },
        { keyNo: 6, session: api.createSession('avocado') }
      ];

      let walletNo = 7;
      for(const { session, keyNo } of sessions) {
        api.create(session, walletNames[walletNo]);
        api.importKey(session, walletNames[walletNo], keys[keyNo][0]);
        ++walletNo;
      }

      for(const { session } of sessions)
        publicKeys.push(api.getPublicKeys(session).keys);

      return publicKeys;
    }, WALLET_OPTIONS_NODE, walletNames, keys);

    expect(retVal).toStrictEqual([
      [ { public_key: "STM6Pg5jd1w8rXgGoqvpZXy1tHPdz43itPW6L2AGJuw8kgSAbtsxm" } ],
      [ { public_key: "STM6TqSJaS1aRj6p6yZEo5xicX7bvLhrfdVqi5ToNrKxHU3FRBEdW" } ],
      [ { public_key: "STM8LbCRyqtXk5VKbdFwK1YBgiafqprAd7yysN49PnDwAsyoMqQME" } ]
    ]);
  });

  test('Create 4 new sessions. All sessions create a wallet', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, walletNames, keys) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      const sessions = [
        api.createSession('avocado'),
        api.createSession('avocado'),
        api.createSession('avocado'),
        api.createSession('avocado')
      ];

      const publicKeys: { public_key: string; }[][] = [];

      sessions.forEach((session, index) => {
        api.create_with_password(session, walletNames[index], 'pass');

        const keysToCheck: number[] = [];

        if(index % 2 === 0) {
          api.importKey(session, walletNames[index], keys[index][0]);
          keysToCheck.push(0);
        }

        const keyArr = api.getPublicKeys(session).keys;

        if (keyArr.length !== 0)
          publicKeys.push(keyArr);
      });

      return publicKeys;
    }, WALLET_OPTIONS_NODE, walletNames, keys);

    expect(retVal).toStrictEqual([
      [ { public_key: "STM5RqVBAVNp5ufMCetQtvLGLJo7unX9nyCBMMrTXRWQ9i1Zzzizh" } ],
      [ { public_key: "STM7j1orEPpWp4bU2SuH46eYXuXkFKEMeJkuXkZVJSaru2zFDGaEH" } ]
    ]);
  });

  test('Try to create new sessions, but due to the limit creating the last one fails', async ({ beekeeperWasmTest }) => {
    await expect(beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      const noSessions = 63; // + 1 implicit session that we create in run_node_helper.

        for(let i = 0; i < noSessions; ++i)
          api.createSession('xyz');

      try {
        api.createSession('xyz');
      } catch {
        throw new Error('Session limit exceeded');
      }
    }, WALLET_OPTIONS_NODE)).rejects.toThrowError('Session limit exceeded');
  });

  test('Close implicitly created session. Create new sessions, unlock 10 wallets, add 10 keys to every wallet and then remove them', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, walletNames, keys) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      const noSessions = 63; // + 1 implicit session that we create in run_node_helper.

      // Destroy beekeeper without sessions' closing
      api.closeSession(api.implicitSessionToken);

      const noWallets = Math.min(10, walletNames.length);
      const noKeys = Math.max(10, keys.length);

      let sessionToken!: string;

      for(let i = 0; i < noSessions; ++i) {
        sessionToken = api.createSession(i.toString());

        api.create_with_password(sessionToken, `w${i}`, 'pass');

        for(let k = 0; k < noKeys; ++k) {
          api.importKey(sessionToken, `w${i}`, keys[k][0]);
          api.removeKey(sessionToken, `w${i}`, keys[k][1]);
        }
      }

      const publicKeys = api.getPublicKeys(sessionToken);

      api.deleteInstance();

      return publicKeys.keys;
    }, WALLET_OPTIONS_NODE, walletNames, keys);

    expect(retVal).toStrictEqual([]);
  });

  test('Create new sessions, close them + implicitly created session. Create again sessions and close them', async ({ beekeeperWasmTest }) => {
    await expect(beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);
      const noSessions = 64;

      const sessionTokens = [ api.implicitSessionToken ];

      for(let i = 0; i < noSessions - 1; ++i)
        sessionTokens.push(api.createSession(i.toString()));

      for(let i = 0; i < noSessions; ++i)
        api.closeSession(sessionTokens.pop()!);

      for(let i = 0; i < noSessions; ++i)
        sessionTokens.push(api.createSession(i.toString()));

      for(let i = 0; i < noSessions; ++i)
        api.closeSession(sessionTokens.pop()!);
    }, WALLET_OPTIONS_NODE)).resolves.toBeUndefined();
  });

  test('Try to sign transactions, but they fail. Unlock wallet. Import key. Sign transactions. Delete instance. Create instance again', async ({ beekeeperWasmTest }) => {
    await expect(beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, walletNames, signData, keys) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      try {
        api.signDigest(api.implicitSessionToken, signData[0].sig_digest, signData[0].public_key);
        api.signDigest(api.implicitSessionToken, signData[1].sig_digest, signData[1].public_key);

        return false;
      } catch {
        // console.log('Expected error here - do nothing');

        api.create_with_password(api.implicitSessionToken, walletNames[3], 'pass');
        const key = api.importKey(api.implicitSessionToken, walletNames[3], keys[3][0]);

        api.signDigest(api.implicitSessionToken, signData[0].sig_digest, key);
        api.signDigest(api.implicitSessionToken, signData[1].sig_digest, key);

        return true;
      } finally {
        api.deleteInstance();
      }
    }, WALLET_OPTIONS_NODE, walletNames, signData, keys)).resolves.toBeTruthy();
  });

  test('Unlock 10 wallets. Every wallet has own session. Import the same key into every wallet. Sign transactions using the key from every wallet', async ({ beekeeperWasmTest }) => {
    await expect(beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, keys, walletNames, signData) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      // Lock even wallets. Sign transactions using the key from: every odd wallet (passes), every even wallet (fails).
      // Lock odd wallets. Sign transactions using the key from every wallet (always fails).

      const noWallets = 10;

      const sessionTokens: string[] = []

      for(let i = 0; i < noWallets; ++i) {
        sessionTokens.push(api.createSession(i.toString()));

        api.create_with_password(sessionTokens[i], walletNames[i], 'pass');
        const key = api.importKey(sessionTokens[i], walletNames[i], keys[3][0]);

        api.signDigest(sessionTokens[i], signData[1].sig_digest, key);
      }

      for(let i = 0; i < noWallets; i += 2)
        api.lock(sessionTokens[i], walletNames[i]);

      for(let i = 0; i < noWallets; ++i)
        if(i % 2 === 0)
          try {
            const key = api.importKey(sessionTokens[i], walletNames[i], keys[3][0]);
            api.signDigest(sessionTokens[i], signData[1].sig_digest, key);

            return false
          } catch {
            // console.log('Expected error here - do nothing');
          }
        else {
          const key = api.importKey(sessionTokens[i], walletNames[i], keys[3][0])
          api.signDigest(sessionTokens[i], signData[1].sig_digest, key);
          api.lock(sessionTokens[i], walletNames[i]); // For the next stage
        }

      return true;
    }, WALLET_OPTIONS_NODE, keys, walletNames, signData)).resolves.toBeTruthy();
  });

  test('Check endpoints related to timeout: "set_timeout", "get_info"', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      api.setTimeout(api.implicitSessionToken, 3600);
      const walletInfo = api.getInfo(api.implicitSessionToken);
      const now = new Date(walletInfo.now);
      const timeoutTime = new Date(walletInfo.timeout_time);

      const timeCheck = timeoutTime.getTime() - now.getTime();

      return timeCheck;
    }, WALLET_OPTIONS_NODE);

    expect(retVal).toEqual(3600 * 1000)
  });

  test('Different false tests for sign* endpoints', async ({ beekeeperWasmTest }) => {
    await expect(beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, walletNames, keys) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      const walletNo = 9;
      api.create_with_password(api.implicitSessionToken, walletNames[walletNo], 'pass');

      const length = 1e7; // 10M
      const longStr = 'a'.repeat(length);
      // This should not throw as it is used only for signing (does not deserialize data)
      const key = api.importKey(api.implicitSessionToken, walletNames[walletNo], keys[3][0])
      api.signDigest(api.implicitSessionToken, longStr, key);
    }, WALLET_OPTIONS_NODE, walletNames, keys)).resolves.toBeUndefined();
  });

  test('Check if an instance of beekeeper has a version', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      return api.version;
    }, WALLET_OPTIONS_NODE);

    // Should be a commit hash
    expect(retVal).toMatch(/^[a-f0-9]{40}$/);
  });

  test('Should throw as wallet name cannot be empty', async ({ beekeeperWasmTest }) => {
    await expect(beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      api.open(api.implicitSessionToken, '');
    }, WALLET_OPTIONS_NODE)).rejects.toThrowError('Name of wallet is incorrect. Is empty.');
  });

  test('Should throw as wallet name contains invalid characters', async ({ beekeeperWasmTest }) => {
    await expect(beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      api.open(api.implicitSessionToken, '%$%$');
    }, WALLET_OPTIONS_NODE)).rejects.toThrowError("Name of wallet is incorrect. Name: %$%$. Only alphanumeric and '._-@' chars are allowed");
  });

  test('Should throw as wallet with the same name already exists', async ({ beekeeperWasmTest }) => {
    await expect(beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, walletNames) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      api.create(api.implicitSessionToken, walletNames[9]);

      api.create(api.implicitSessionToken, walletNames[9]);
    }, WALLET_OPTIONS_NODE, walletNames)).rejects.toThrowError(`Wallet with name: 'w9' already exists at`);
  });

  test('Should throw as the password is empty', async ({ beekeeperWasmTest }) => {
    await expect(beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, walletNames) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      api.create_with_password(api.implicitSessionToken, walletNames[9], '');
    }, WALLET_OPTIONS_NODE, walletNames)).rejects.toThrowError('password.size() > 0');
  });

  test('Should throw as the password is too long', async ({ beekeeperWasmTest }) => {
    await expect(beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, walletNames) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      const length = 1e7; // 10M
      const longStr = 'a'.repeat(length);
      api.create_with_password(api.implicitSessionToken, walletNames[9], longStr);
    }, WALLET_OPTIONS_NODE, walletNames)).rejects.toThrowError('!password || password->size() < max_password_length');
  });

  test('Should throw as the wallet does not exist', async ({ beekeeperWasmTest }) => {
    await expect(beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      api.close(api.implicitSessionToken, 'abc');
    }, WALLET_OPTIONS_NODE)).rejects.toThrowError('Wallet not found: abc');
  });

  test('Should throw as there is no opened wallet', async ({ beekeeperWasmTest }) => {
    await expect(beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      api.getPublicKeys(api.implicitSessionToken);
    }, WALLET_OPTIONS_NODE)).rejects.toThrowError("You don't have any wallet");
  });

  test('Should throw as the wallet is locked', async ({ beekeeperWasmTest }) => {
    await expect(beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, walletNames) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      api.open(api.implicitSessionToken, walletNames[9]);
      api.getPublicKeys(api.implicitSessionToken);
    }, WALLET_OPTIONS_NODE, walletNames)).rejects.toThrowError(/Unable to open file:.*?w9\.wallet/);
  });

  test('Should throw as the wallet is already locked', async ({ beekeeperWasmTest }) => {
    await expect(beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, walletNames) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      api.lock(api.implicitSessionToken, walletNames[9]);
      api.lock(api.implicitSessionToken, walletNames[9]);
    }, WALLET_OPTIONS_NODE, walletNames)).rejects.toThrowError("Wallet not found: w9");
  });

  test('Should throw as the password is incorrect', async ({ beekeeperWasmTest }) => {
    await expect(beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, walletNames) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      api.unlock(api.implicitSessionToken, walletNames[9], 'incorrect');
    }, WALLET_OPTIONS_NODE, walletNames)).rejects.toThrowError(/Unable to open file:.*?w9\.wallet/);
  });

  test('Should throw as the wallet is already unlocked', async ({ beekeeperWasmTest }) => {
    await expect(beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, walletNames) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      api.create_with_password(api.implicitSessionToken, walletNames[9], 'pass');

      api.unlock(api.implicitSessionToken, walletNames[9], 'pass');
    }, WALLET_OPTIONS_NODE, walletNames)).rejects.toThrowError("Wallet is already unlocked");
  });

  test('Should throw as the wallet is not found', async ({ beekeeperWasmTest }) => {
    await expect(beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      api.importKey(api.implicitSessionToken, 'pear', 'key');
    }, WALLET_OPTIONS_NODE)).rejects.toThrowError("Wallet not found: pear");
  });

  test('Should throw as the key is invalid', async ({ beekeeperWasmTest }) => {
    await expect(beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, walletNames) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      api.create_with_password(api.implicitSessionToken, walletNames[9], 'pass');

      api.importKey(api.implicitSessionToken, walletNames[9], 'key');
    }, WALLET_OPTIONS_NODE, walletNames)).rejects.toThrowError("Key can't be constructed");
  });

  test('Should throw as the wallet cannot be found', async ({ beekeeperWasmTest }) => {
    await expect(beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      api.removeKey(api.implicitSessionToken, 'nonexisting-wallet', 'STM8FDsHdPkHbY8fuUkVLyAmrnKMvj6DddLopi3YJ51dVqsG9vZa4');
    }, WALLET_OPTIONS_NODE)).rejects.toThrowError("Wallet not found: nonexisting-wallet");
  });

  test('Should throw as the key does not have STM prefix', async ({ beekeeperWasmTest }) => {
    await expect(beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, walletNames) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      api.create_with_password(api.implicitSessionToken, walletNames[9], 'pass');

      api.removeKey(api.implicitSessionToken, walletNames[9], '6Pg5jd1w8rXgGoqvpZXy1tHPdz43itPW6L2AGJuw8kgSAbtsxm');
    }, WALLET_OPTIONS_NODE, walletNames)).rejects.toThrowError("public key requires STM prefix, but was given");
  });

  test('Should throw as the key is not in the wallet', async ({ beekeeperWasmTest }) => {
    await expect(beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, walletNames) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      api.create_with_password(api.implicitSessionToken, walletNames[9], 'pass');

      api.removeKey(api.implicitSessionToken, walletNames[9], 'STM8FDsHdPkHbY8fuUkVLyAmrnKMvj6DddLopi3YJ51dVqsG9vZa4');
    }, WALLET_OPTIONS_NODE, walletNames)).rejects.toThrowError("Key not in wallet");
  });

  test('Should throw because of invalid hex character in the digest', async ({ beekeeperWasmTest }) => {
    await expect(beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      api.signDigest(api.implicitSessionToken, "#", "STM6Pg5jd1w8rXgGoqvpZXy1tHPdz43itPW6L2AGJuw8kgSAbtsxm");
    }, WALLET_OPTIONS_NODE)).rejects.toThrowError("Invalid hex character '#'");
  });

  test('Should throw as the base58 cannot be decoded', async ({ beekeeperWasmTest }) => {
    await expect(beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      api.signDigest(api.implicitSessionToken, "abCDe", "STMlemon");
    }, WALLET_OPTIONS_NODE)).rejects.toThrowError("Unable to decode base58 string lemon");
  });

  test('Should throw as the public key is not found in the unlocked wallets', async ({ beekeeperWasmTest }) => {
    await expect(beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      api.signDigest(api.implicitSessionToken, "abCDe", "STM6Pg5jd1w8rXgGoqvpZXy1tHPdz43itPW6L2AGJuw8kgSAbtsxm");
    }, WALLET_OPTIONS_NODE)).rejects.toThrowError("Public key STM6Pg5jd1w8rXgGoqvpZXy1tHPdz43itPW6L2AGJuw8kgSAbtsxm not found in unlocked wallets");
  });

  test('An incorrect initialization should block all API calls', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ provider }) => {
        // We are unable to disable logs for this test
        const params = new provider.StringList();
        params.push_back("--invalid-param");
        params.push_back("true");
        params.push_back("--something-else");
        params.push_back("667");

        const instance = new provider.beekeeper_api(params)
        return instance.init() as string;
    });

    expect(retVal).toBe("{\"error\":\"\\\"parameters: (--invalid-param true --something-else 667 ) Throw location unknown (consider using BOOST_THROW_EXCEPTION)\\\\nDynamic exception type: boost::wrapexcept<boost::program_options::unknown_option>\\\\nstd::exception::what: unrecognised option\\\\n\\\"\"}");
  });

  test('Check `has_matching_private_key` endpoint', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, walletNames, keys) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      api.create_with_password(api.implicitSessionToken, walletNames[9], 'pass');

      const hasBeforeImport = api.hasMatchingPrivateKey(api.implicitSessionToken, walletNames[9], keys[0][1]);

      api.importKey(api.implicitSessionToken, walletNames[9], keys[0][0]);

      const hasAfterImport = api.hasMatchingPrivateKey(api.implicitSessionToken, walletNames[9], keys[0][1]);

      return {
        hasBeforeImport,
        hasAfterImport
      }
    }, WALLET_OPTIONS_NODE, walletNames, keys);

    expect(retVal.hasBeforeImport).toBeFalsy();
    expect(retVal.hasAfterImport).toBeTruthy();
  });

  test('Check `encrypt_data` endpoint when wallet not found', async ({ beekeeperWasmTest }) => {
    await expect(beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, keys, walletNames) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      api.encryptData(api.implicitSessionToken, keys[0][1], keys[1][1], walletNames[9], 'content');
    }, WALLET_OPTIONS_NODE, keys, walletNames)).rejects.toThrowError("Wallet not found: w9");
  });

  test('Check `encrypt_data` endpoint when public key not found in the wallet', async ({ beekeeperWasmTest }) => {
    await expect(beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, keys, walletNames) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      api.create_with_password(api.implicitSessionToken, walletNames[9], 'pass');

      api.encryptData(api.implicitSessionToken, keys[0][1], keys[1][1], walletNames[9], 'content');
    }, WALLET_OPTIONS_NODE, keys, walletNames)).rejects.toThrowError("Public key STM5RqVBAVNp5ufMCetQtvLGLJo7unX9nyCBMMrTXRWQ9i1Zzzizh not found in w9 wallet");
  });

  test('Should encrypt and decrypt data', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, keys, walletNames) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      api.create_with_password(api.implicitSessionToken, walletNames[9], 'pass');

      const fromKey = api.importKey(api.implicitSessionToken, walletNames[9], keys[0][0]);
      const toKey = api.importKey(api.implicitSessionToken, walletNames[9], keys[1][0]);

      const encrypted = api.encryptData(api.implicitSessionToken, fromKey, toKey, walletNames[9], 'content');
      const decrypted = api.decryptData(api.implicitSessionToken, fromKey, toKey, walletNames[9], encrypted);

      return decrypted;
    }, WALLET_OPTIONS_NODE, keys, walletNames);

    expect(retVal).toBe('content');
  });

  test.afterAll(async () => {
    await browser.close();
  });
});
