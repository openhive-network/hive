"use strict";

import { strict as assert } from 'node:assert';
import path from 'path';

import { setTimeout } from 'timers/promises';
import { fileURLToPath } from 'url';

import Module from '../../../../build_wasm/beekeeper_wasm.mjs';

import BeekeeperInstanceHelper, { ExtractError } from './run_node_helper.mjs';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const walletNames = ["w0","w1","w2","w3","w4","w5","w6","w7","w8","w9"];

// const limitDisplay = (str, max) => str.length > max ? (str.slice(0, max - 3) + '...') : str;

const keys =
[
  ['5JkFnXrLM2ap9t3AmAxBJvQHF7xSKtnTrCTginQCkhzU5S7ecPT', '5RqVBAVNp5ufMCetQtvLGLJo7unX9nyCBMMrTXRWQ9i1Zzzizh'],
  ['5KGKYWMXReJewfj5M29APNMqGEu173DzvHv5TeJAg9SkjUeQV78', '6oR6ckA4TejTWTjatUdbcS98AKETc3rcnQ9dWxmeNiKDzfhBZa'],
  ['5KNbAE7pLwsLbPUkz6kboVpTR24CycqSNHDG95Y8nbQqSqd6tgS', '7j1orEPpWp4bU2SuH46eYXuXkFKEMeJkuXkZVJSaru2zFDGaEH'],
  ['5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n', '6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4'],
  ['5J8C7BMfvMFXFkvPhHNk2NHGk4zy3jF4Mrpf5k5EzAecuuzqDnn', '6Pg5jd1w8rXgGoqvpZXy1tHPdz43itPW6L2AGJuw8kgSAbtsxm'],
  ['5J15npVK6qABGsbdsLnJdaF5esrEWxeejeE3KUx6r534ug4tyze', '6TqSJaS1aRj6p6yZEo5xicX7bvLhrfdVqi5ToNrKxHU3FRBEdW'],
  ['5K1gv5rEtHiACVTFq9ikhEijezMh4rkbbTPqu4CAGMnXcTLC1su', '8LbCRyqtXk5VKbdFwK1YBgiafqprAd7yysN49PnDwAsyoMqQME'],
  ['5KLytoW1AiGSoHHBA73x1AmgZnN16QDgU1SPpG9Vd2dpdiBgSYw', '8FDsHdPkHbY8fuUkVLyAmrnKMvj6DddLopi3YJ51dVqsG9vZa4'],
  ['5KXNQP5feaaXpp28yRrGaFeNYZT7Vrb1PqLEyo7E3pJiG1veLKG', '6a34GANY5LD8deYvvfySSWGd7sPahgVNYoFPapngMUD27pWb45'],
  ['5KKvoNaCPtN9vUEU1Zq9epSAVsEPEtocbJsp7pjZndt9Rn4dNRg', '8mmxXz5BfQc2NJfqhiPkbgcyJm4EvWEr2UAUdr56gEWSN9ZnA5']
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

/**
 * @param {BeekeeperInstanceHelper} beekeeperInstance
 * @param {string} sessionToken
 * @param {number} signDataIndex
 */
const signTransactionUsing3Methods = (beekeeperInstance, sessionToken, signDataIndex) => {
  const data = signData[signDataIndex];
  const expected = data.expected_signature;

  const signDigest = beekeeperInstance.signDigest(sessionToken, data.sig_digest, data.public_key);
  assert.equal(expected, signDigest);

  const signBinaryTx = beekeeperInstance.signBinaryTransaction(sessionToken, data.binary_transaction_body, chainId, data.public_key);
  assert.equal(expected, signBinaryTx);

  const signTx = beekeeperInstance.signTransaction(sessionToken, data.transaction_body, chainId, data.public_key);
  assert.equal(expected, signTx);
};

let testThrowId = 0;
const testThrow = (api, what, ...explicitArgsOrArgsNum) => {
  if(typeof api === 'function') {
    assert.throws(() => {
      api();
    }, ExtractError);

    console.info(`Call to \`[Function (anonymous)]\` successfully failed`);

    return;
  }

  let args = [];

  if(typeof explicitArgsOrArgsNum[0] === 'number')
    for(let i = 0; i < explicitArgsOrArgsNum[0]; ++i)
      args.push(`random_arg_${i}`);
  else
    args = explicitArgsOrArgsNum;

  const token = `this-token-does-not-exist-${++testThrowId}`;

  assert.throws(() => {
    api[what](token, ...args);
  }, ExtractError);

  console.info(`Call to \`api.${what}('${token}', ...);\` successfully failed`);
};

const requireKeysIn = (keysResult, ...indexes) => {
  assert.equal(keysResult.keys.length, indexes.length, "Invalid number of keys in result");
  indexes.map(index => keys[index][1]).forEach(key => {
    assert.ok(keysResult.keys.find(value => value.public_key === key), "Key does not exist in wallet");
  });
};

const requireWalletsIn = (walletsResult, ...indexes) => {
  assert.equal(walletsResult.wallets.length, indexes.length / 2, "Invalid number of wallets in result");
  for(let i = 0; i < indexes.length; i += 2) {
    const key = walletNames[indexes[i]];
    const unlocked = !!indexes[i + 1];

    const wallet = walletsResult.wallets.find(value => value.name === key);

    assert.ok(wallet, "Wallet does not exist");
    assert.equal(wallet.unlocked, unlocked, "Wallet should match open state");
  }
};

const performWalletAutolockTest = async(beekeeperInstance, sessionToken) => {
  beekeeperInstance.setTimeout(sessionToken, 1);

  const start = Date.now();

  console.log('waiting 1.5s...');

  await setTimeout( 1500, 'timed-out');

  const interval = Date.now() - start;

  console.log(`Timer resumed after ${interval} ms`);
  const wallets = beekeeperInstance.listWallets(sessionToken);
  requireWalletsIn(wallets, 1, false, 2, false);
};

const chainId = '18dcf0a285365fc58b71f18b3d3fec954aa0c141c44e4e5cb4cf777b9eab274e';

const STORAGE_ROOT = path.join(__dirname, 'storage_root');

const my_entrypoint = async() => {
  const provider = await Module();

  console.log(`Storage root for testing is: ${STORAGE_ROOT}`);

  const args = `--wallet-dir ${STORAGE_ROOT}/.beekeeper`.split(' ');

  const beekeper = BeekeeperInstanceHelper.for(provider);

  {
    /** @type {BeekeeperInstanceHelper} */
    const api = new beekeper(args);

    console.log();
    console.log("**************************************************************************************");
    console.log("Basic tests which execute all endpoints");
    console.log("**************************************************************************************");

    api.create(api.implicitSessionToken, walletNames[0]);

    api.importKey(api.implicitSessionToken, walletNames[0], keys[0][0]);

    {
      const sessionToken = api.createSession('pear');
      api.closeSession(sessionToken);
    }

    {
      const sessionToken = api.createSession('pear');

      api.create_with_password(sessionToken, walletNames[1], 'cherry');

      api.create(sessionToken, walletNames[2]);

      api.importKey(sessionToken, walletNames[1], keys[1][0]);
      api.importKey(sessionToken, walletNames[1], keys[2][0]);
      api.importKey(sessionToken, walletNames[1], keys[3][0]);

      {
        const wallets = api.listWallets(sessionToken);
        requireWalletsIn(wallets, 1, true, 2, true);

        const publicKeys = api.getPublicKeys(sessionToken);
        requireKeysIn(publicKeys, 1, 2, 3);
      }

      {
        api.close(sessionToken, walletNames[2]);

        const wallets = api.listWallets(sessionToken);
        requireWalletsIn(wallets, 1, true);
      }

      {
        api.open(sessionToken, walletNames[2]);

        const wallets = api.listWallets(sessionToken);
        requireWalletsIn(wallets, 1, true, 2, false);
      }

      {
        api.unlock(sessionToken, walletNames[2]);

        const wallets = api.listWallets(sessionToken);
        requireWalletsIn(wallets, 1, true, 2, true);
      }

      {
        api.lock(sessionToken, walletNames[2]);

        const wallets = api.listWallets(sessionToken);
        requireWalletsIn(wallets, 1, true, 2, false);
      }

      {
        api.lockAll(sessionToken);

        const wallets = api.listWallets(sessionToken);
        requireWalletsIn(wallets, 1, false, 2, false);
      }

      {
        api.unlock(sessionToken, walletNames[1]);

        const wallets = api.listWallets(sessionToken);
        requireWalletsIn(wallets, 1, true, 2, false);

        api.removeKey(sessionToken, walletNames[1], keys[2][1], null);

        const publicKeys = api.getPublicKeys(sessionToken);
        requireKeysIn(publicKeys, 1, 3);
      }

      const walletInfo = api.getInfo(sessionToken);
      const now = new Date(walletInfo.now);
      const timeoutTime = new Date(walletInfo.timeout_time);
      assert.ok(!Number.isNaN(now.getTime()));
      assert.ok(!Number.isNaN(timeoutTime.getTime()));

      signTransactionUsing3Methods(api, sessionToken, 0);
      signTransactionUsing3Methods(api, sessionToken, 1);

      await performWalletAutolockTest(api, sessionToken);

      api.deleteInstance();
    }
  }

  {
    console.log();
    console.log("**************************************************************************************");
    console.log("Trying to open beekeeper instance again so as to find out if data created in previous test is permamently saved");
    console.log("**************************************************************************************");

    /** @type {BeekeeperInstanceHelper} */
    const api = new beekeper(args);

    api.create(api.implicitSessionToken, walletNames[3]);
    api.unlock(api.implicitSessionToken, walletNames[1]);
    api.importKey(api.implicitSessionToken, walletNames[3], keys[9][0]);

    const publicKeys = api.getPublicKeys(api.implicitSessionToken);
    requireKeysIn(publicKeys, 1, 3, 9);
  }

  {
    console.log();
    console.log("**************************************************************************************");
    console.log("Create 3 wallets and add to every wallet 3 the same keys. Every key should be displayed only once");
    console.log("**************************************************************************************");

    /** @type {BeekeeperInstanceHelper} */
    const api = new beekeper(args);

    api.create(api.implicitSessionToken, walletNames[4]);
    api.create(api.implicitSessionToken, walletNames[5]);
    api.create(api.implicitSessionToken, walletNames[6]);

    // Import keys 7-9 to wallets 4-6
    for(let walletNo = 4; walletNo <= 6; ++walletNo)
      for(let keyNo = 7; keyNo <= 9; ++keyNo)
        api.importKey(api.implicitSessionToken, walletNames[walletNo], keys[keyNo][0]);

    const publicKeys = api.getPublicKeys(api.implicitSessionToken);
    requireKeysIn(publicKeys, 7, 8, 9);
  }

  {
    console.log();
    console.log("**************************************************************************************");
    console.log("Remove all keys from 3 wallets created in previous step. As a result all keys are removed in mentioned wallets.");
    console.log("**************************************************************************************");

    /** @type {BeekeeperInstanceHelper} */
    const api = new beekeper(args);

    api.unlock(api.implicitSessionToken, walletNames[4]);
    api.unlock(api.implicitSessionToken, walletNames[5]);
    api.unlock(api.implicitSessionToken, walletNames[6]);

    // Remove keys 7-9 to wallets 4-6
    for(let walletNo = 4; walletNo <= 6; ++walletNo)
      for(let keyNo = 7; keyNo <= 9; ++keyNo)
        api.removeKey(api.implicitSessionToken, walletNames[walletNo], keys[keyNo][1]);

    const publicKeys = api.getPublicKeys(api.implicitSessionToken);
    requireKeysIn(publicKeys);
  }

  {
    console.log();
    console.log("**************************************************************************************");
    console.log("Close implicitly created session. Create 3 new sessions. Every session has a 1 wallet. Every wallet has unique 1 key. As a result there are 3 keys.");
    console.log("**************************************************************************************");

    /** @type {BeekeeperInstanceHelper} */
    const api = new beekeper(args);

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

    for(const { session, keyNo } of sessions) {
      const publicKeys = api.getPublicKeys(session);
      requireKeysIn(publicKeys, keyNo);
    }
  }

  {
    console.log();
    console.log("**************************************************************************************");
    console.log("Unlock 10 wallets. From every wallet remove every key.");
    console.log("Removing is done by passing always all public keys from `keys` set. Sometimes `remove` operation passes, sometimes (mostly) fails.");
    console.log("**************************************************************************************");

    /** @type {BeekeeperInstanceHelper} */
    const api = new beekeper(args);

    for(const name of walletNames)
      api.unlock(api.implicitSessionToken, name);

    let publicKeys = api.getPublicKeys(api.implicitSessionToken);
    requireKeysIn(publicKeys, 0, 1, 3, 4, 5, 6, 9);

    /*
      api.removeKey(api.implicitSessionToken, walletNames[0], keys[0][1]);
      api.removeKey(api.implicitSessionToken, walletNames[1], keys[1][1]);
      api.removeKey(api.implicitSessionToken, walletNames[1], keys[3][1]);
      api.removeKey(api.implicitSessionToken, walletNames[7], keys[4][1]);
      api.removeKey(api.implicitSessionToken, walletNames[8], keys[5][1]);
      api.removeKey(api.implicitSessionToken, walletNames[9], keys[6][1]);
      api.removeKey(api.implicitSessionToken, walletNames[3], keys[9][1]);
    */

    let keysRemoved = 0;
    for(const name of walletNames)
      for(const [ , key ] of keys)
        try {
          api.removeKey(api.implicitSessionToken, name, key);
          ++keysRemoved;
        } catch {
          console.info('Key could not be removed, because it does not belong to this wallet. You can ignore this error.');
        }

    assert.equal(keysRemoved, 7);

    {
      const publicKeys = api.getPublicKeys(api.implicitSessionToken);
      requireKeysIn(publicKeys);
    }
  }

  {
    console.log();
    console.log("**************************************************************************************");
    console.log("Close implicitly created session. Create 4 new sessions. All sessions open the same wallet.");
    console.log("Sessions{0,2} import a key, sessions{1,3} try to remove a key (here is an exception ('Key not in wallet')).");
    console.log("As a result only sessions{0,2} have a key.");
    console.log("**************************************************************************************");

    /** @type {BeekeeperInstanceHelper} */
    const api = new beekeper(args);

    api.closeSession(api.implicitSessionToken);

    const sessions = [
      api.createSession('avocado'),
      api.createSession('avocado'),
      api.createSession('avocado'),
      api.createSession('avocado')
    ];

    sessions.forEach((session, index) => {
      api.unlock(session, walletNames[0]);

      const keysToCheck = [];

      if(index % 2 === 0) {
        api.importKey(session, walletNames[0], keys[0][0]);
        keysToCheck.push(0);
      } else
        api.removeKey(session, walletNames[0], keys[0][1]);

      requireKeysIn(api.getPublicKeys(session), ...keysToCheck);
    });
  }

  {
    console.log();
    console.log("**************************************************************************************");
    console.log("False tests for every API endpoint.");
    console.log("**************************************************************************************");

    /** @type {BeekeeperInstanceHelper} */
    const api = new beekeper(args);

    testThrow(api, 'closeSession');
    testThrow(api, 'close', 1);
    testThrow(api, 'open', 1);
    testThrow(api, 'unlock', 2);
    testThrow(api, 'create', 2);
    testThrow(api, 'setTimeout', 1);
    testThrow(api, 'lockAll');
    testThrow(api, 'lock', 1);
    testThrow(api, 'importKey', 2);
    testThrow(api, 'removeKey', 3);
    testThrow(api, 'listWallets');
    testThrow(api, 'getPublicKeys');
    testThrow(
      api,
      'signDigest',
      '390f34297cfcb8fa4b37353431ecbab05b8dc0c9c15fb9ca1a3d510c52177542',
      '6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4',
      '1f17cc07f7c769073d39fac3385220b549e261fb33c5f619c5dced7f5b0fe9c0954f2684e703710840b7ea01ad7238b8db1d8a9309d03e93de212f86de38d66f21'
    );
    testThrow(
      api,
      'signDigest',
      '390f34297cfcb8fa4b37353431ecbab05b8dc0c9c15fb9ca1a3d510c52177542',
      'this is not public key',
      '1f17cc07f7c769073d39fac3385220b549e261fb33c5f619c5dced7f5b0fe9c0954f2684e703710840b7ea01ad7238b8db1d8a9309d03e93de212f86de38d66f21'
    );
    testThrow(
      api,
      'signDigest',
      'this is not digest of transaction',
      '6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4',
      '1f17cc07f7c769073d39fac3385220b549e261fb33c5f619c5dced7f5b0fe9c0954f2684e703710840b7ea01ad7238b8db1d8a9309d03e93de212f86de38d66f21'
    );
    testThrow(
      api,
      'signBinaryTransaction',
      '000000000000000000000000',
      chainId,
      '6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4',
      '1f17cc07f7c769073d39fac3385220b549e261fb33c5f619c5dced7f5b0fe9c0954f2684e703710840b7ea01ad7238b8db1d8a9309d03e93de212f86de38d66f21'
    );
    testThrow(
      api,
      'signBinaryTransaction',
      '000000000000000000000000',
      chainId,
      '6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4',
      '1f17cc07f7c769073d39fac3385220b549e261fb33c5f619c5dced7f5b0fe9c0954f2684e703710840b7ea01ad7238b8db1d8a9309d03e93de212f86de38d66f21'
    );

    {
      api.unlock(api.implicitSessionToken, walletNames[0]);
      api.importKey(api.implicitSessionToken, walletNames[0], keys[0][0]);
      testThrow(
        api,
        'signBinaryTransaction',
        'really! it means nothing!!!!! this is not a transaction body',
        chainId,
        keys[0][1],
        '1f17cc07f7c769073d39fac3385220b549e261fb33c5f619c5dced7f5b0fe9c0954f2684e703710840b7ea01ad7238b8db1d8a9309d03e93de212f86de38d66f21'
      );
      api.removeKey(api.implicitSessionToken, walletNames[0], keys[0][1]);
    }

    testThrow(
      api,
      'signTransaction',
      '{}',
      chainId,
      '6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4',
      '1f17cc07f7c769073d39fac3385220b549e261fb33c5f619c5dced7f5b0fe9c0954f2684e703710840b7ea01ad7238b8db1d8a9309d03e93de212f86de38d66f21'
    );
    testThrow(
      api,
      'signTransaction',
      'a body of transaction without any sense',
      chainId,
      '6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4',
      '1f17cc07f7c769073d39fac3385220b549e261fb33c5f619c5dced7f5b0fe9c0954f2684e703710840b7ea01ad7238b8db1d8a9309d03e93de212f86de38d66f21'
    );
    testThrow(
      api,
      'signTransaction',
      'a body of transaction without any sense',
      '+++++',
      '6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4',
      '1f17cc07f7c769073d39fac3385220b549e261fb33c5f619c5dced7f5b0fe9c0954f2684e703710840b7ea01ad7238b8db1d8a9309d03e93de212f86de38d66f21'
    );
    testThrow(
      api,
      'signTransaction',
      'a body of transaction without any sense',
      chainId,
      '&&&&',
      '1f17cc07f7c769073d39fac3385220b549e261fb33c5f619c5dced7f5b0fe9c0954f2684e703710840b7ea01ad7238b8db1d8a9309d03e93de212f86de38d66f21'
    );
  }

  {
    const noSessions = 63; //63 because at the beginning we have implicitly created session
    console.log();
    console.log("**************************************************************************************");
    console.log(`Try to create ${noSessions + 2} new sessions, but due to the limit creating the last one fails.`);
    console.log("**************************************************************************************");

    /** @type {BeekeeperInstanceHelper} */
    const api = new beekeper(args);

    for(let i = 0; i < noSessions; ++i)
      api.createSession('xyz');

    assert.throws(() => {
      api.createSession('xyz');
    }, ExtractError);
  }
  {
    const noSessions = 63;
    console.log();
    console.log("**************************************************************************************");
    console.log(`Close implicitly created session. Create ${noSessions} new sessions, unlock 10 wallets, add 10 keys to every wallet and then remove them.`);
    console.log("Destroy beekeeper without sessions' closing.");
    console.log("**************************************************************************************");

    /** @type {BeekeeperInstanceHelper} */
    const api = new beekeper(args);

    api.closeSession(api.implicitSessionToken);

    const noWallets = Math.min(10, walletNames.length);
    const noKeys = Math.max(10, keys.length);

    let sessionToken;

    for(let i = 0; i < noSessions; ++i) {
      sessionToken = api.createSession(i.toString());
      for(let j = 0; j < noWallets; ++j) {
        api.unlock(sessionToken, walletNames[j]);
        for(let k = 0; k < noKeys; ++k) {
          api.importKey(sessionToken, walletNames[j], keys[k][0]);
          api.removeKey(sessionToken, walletNames[j], keys[k][1]);
        }
      }
    }

    requireKeysIn(api.getPublicKeys(sessionToken));

    api.deleteInstance();
  }
  {
    const noSessions = 64;
    console.log();
    console.log("**************************************************************************************");
    console.log(`Create ${noSessions - 1} new sessions, close them + implicitly created session. Create again ${noSessions} and close them.`);
    console.log("**************************************************************************************");

    /** @type {BeekeeperInstanceHelper} */
    const api = new beekeper(args);

    const sessionTokens = [ api.implicitSessionToken ];

    for(let i = 0; i < noSessions - 1; ++i)
      sessionTokens.push(api.createSession(i.toString()));

    for(let i = 0; i < noSessions; ++i)
      api.closeSession(sessionTokens.pop());

    for(let i = 0; i < noSessions; ++i)
      sessionTokens.push(api.createSession(i.toString()));

    for(let i = 0; i < noSessions; ++i)
      api.closeSession(sessionTokens.pop());
  }
  {
    console.log();
    console.log("**************************************************************************************")
    console.log(`Try to sign transactions, but they fail. Unlock wallet. Import key. Sign transactions.`);
    console.log(`Delete instance. Create instance again.`);
    console.log(`Unlock wallet. Sign transactions. Remove key. Try to sign transactions again, but they fail.`);
    console.log("**************************************************************************************");

    /** @type {BeekeeperInstanceHelper} */
    const api = new beekeper(args);

    testThrow(() => { signTransactionUsing3Methods(api, api.implicitSessionToken, 0); });
    testThrow(() => { signTransactionUsing3Methods(api, api.implicitSessionToken, 1); });

    const walletNo = 7;
    const keyNo = 3;

    api.unlock(api.implicitSessionToken, walletNames[walletNo]);
    api.importKey(api.implicitSessionToken, walletNames[walletNo], keys[keyNo][0]);

    // This should not throw now:
    signTransactionUsing3Methods(api, api.implicitSessionToken, 0);
    signTransactionUsing3Methods(api, api.implicitSessionToken, 1);

    api.deleteInstance();

    {
      /** @type {BeekeeperInstanceHelper} */
      const api = new beekeper(args);

      api.unlock(api.implicitSessionToken, walletNames[walletNo]);

      signTransactionUsing3Methods(api, api.implicitSessionToken, 0);
      signTransactionUsing3Methods(api, api.implicitSessionToken, 1);

      api.removeKey(api.implicitSessionToken, walletNames[walletNo], keys[keyNo][1]);

      // Now this should throw
      testThrow(() => { signTransactionUsing3Methods(api, api.implicitSessionToken, 0); });
      testThrow(() => { signTransactionUsing3Methods(api, api.implicitSessionToken, 1); });
    }
  }
  {
    console.log();
    console.log("**************************************************************************************");
    console.log(`Unlock 10 wallets. Every wallet has own session. Import the same key into every wallet. Sign transactions using the key from every wallet.`);
    console.log(`Lock even wallets. Sign transactions using the key from: every odd wallet (passes), every even wallet (fails).`);
    console.log(`Lock odd wallets. Sign transactions using the key from every wallet (always fails).`);
    console.log("**************************************************************************************");

    /** @type {BeekeeperInstanceHelper} */
    const api = new beekeper(args);

    const noWallets = 10;
    const keyNo = 3;

    const sessionTokens = []

    for(let i = 0; i < noWallets; ++i) {
      sessionTokens.push(api.createSession(i.toString()));

      api.unlock(sessionTokens[i], walletNames[i]);
      api.importKey(sessionTokens[i], walletNames[i], keys[keyNo][0]);

      signTransactionUsing3Methods(api, sessionTokens[i], 1);
    }

    for(let i = 0; i < noWallets; i += 2)
      api.lock(sessionTokens[i], walletNames[i]);

    for(let i = 0; i < noWallets; ++i)
      if(i % 2 == 0)
        testThrow(() => { signTransactionUsing3Methods(api, sessionTokens[i], 1) });
      else {
        signTransactionUsing3Methods(api, sessionTokens[i], 1);
        api.lock(sessionTokens[i], walletNames[i]); // For the next stage
      }

    for(let i = 0; i < noWallets; ++i)
      testThrow(() => { signTransactionUsing3Methods(api, sessionTokens[i], 1); });
  }
  {
    console.log();
    console.log("**************************************************************************************");
    console.log(`Check endpoints related to timeout: "set_timeout", "get_info",`);
    console.log("**************************************************************************************");

    /** @type {BeekeeperInstanceHelper} */
    const api = new beekeper(args);

    api.setTimeout(api.implicitSessionToken, 3600);
    const walletInfo = api.getInfo(api.implicitSessionToken);
    const now = new Date(walletInfo.now);
    const timeoutTime = new Date(walletInfo.timeout_time);

    assert.equal(timeoutTime.getTime() - now.getTime(), 3600 * 1000);

    {
      api.setTimeout(api.implicitSessionToken, 0);
      api.setTimeout(api.implicitSessionToken, 'This is not a number of seconds'); // XXX: This should throw

      const walletInfo = api.getInfo(api.implicitSessionToken);
      const now = new Date(walletInfo.now);
      const timeoutTime = new Date(walletInfo.timeout_time);

      assert.equal(timeoutTime.getTime() - now.getTime(), 0);
    }
  }
  {
    console.log();
    console.log("**************************************************************************************");
    console.log("Different false tests for 'sign*' endpoints.");
    console.log("**************************************************************************************");

    /** @type {BeekeeperInstanceHelper} */
    const api = new beekeper(args);

    const walletNo = 9;
    api.unlock(api.implicitSessionToken, walletNames[walletNo]);

    {
      const length = 1e7; // 10M
      const longStr = 'a'.repeat(length);
      // This should not throw as it is used only for signing (does not deserialize data)
      api.signDigest(api.implicitSessionToken, longStr, signData[1].public_key, signData[1].expected_signature);
    }
    {
      const length = 50;
      const longStr = '9'.repeat(length);
      // These should fail as they do not contain valid data
      testThrow(() => {
        api.signBinaryTransaction(api.implicitSessionToken, longStr, signData[1].public_key, signData[1].expected_signature);
      });
      testThrow(() => {
        api.signTransaction(api.implicitSessionToken, longStr, signData[1].public_key, signData[1].expected_signature);
      });
    }
  }

  {
    console.log();
    console.log("**************************************************************************************");
    console.log("Check if an instance of beekeeper has a version.");
    console.log("**************************************************************************************");

    {
      /** @type {BeekeeperInstanceHelper} */
      const api = new beekeper(args);
      console.log(api.version);
      assert.equal(api.version.length > 0, true);
    }
  }

  {
    console.log();
    console.log("**************************************************************************************");
    console.log("Analyze error messages.");
    console.log("**************************************************************************************");

    /** @type {BeekeeperInstanceHelper} */
    const api = new beekeper(args);

    {
      api.setAcceptError = true;

      let error_message = api.open(api.implicitSessionToken, "");
      console.log(error_message);
      assert.equal(error_message.includes("Name of wallet is incorrect. Is empty."), true);

      error_message = api.open(api.implicitSessionToken, "%$%$");
      console.log(error_message);
      assert.equal(error_message.includes("Name of wallet is incorrect. Name: %$%$. Only alphanumeric and '._-' chars are allowed"), true);

      error_message = api.open(api.implicitSessionToken, "abc");
      console.log(error_message);
      assert.equal(error_message.includes("Unable to open file: "), true);
      assert.equal(error_message.includes("abc.wallet"), true);
    }
    {
      api.setAcceptError = true;

      let error_message = api.create_with_password(api.implicitSessionToken, "", "cherry-password");
      console.log(error_message);
      assert.equal(error_message.includes("Name of wallet is incorrect. Is empty."), true);

      error_message = api.create_with_password(api.implicitSessionToken, "%$%$", "cherry-password");
      console.log(error_message);
      assert.equal(error_message.includes("Name of wallet is incorrect. Name: %$%$. Only alphanumeric and '._-' chars are allowed"), true);

      const walletNo = 9;
      error_message = api.create_with_password(api.implicitSessionToken, walletNames[walletNo], "redberry-password");
      console.log(error_message);
      assert.equal(error_message.includes("Wallet with name: 'w9' already exists at"), true);

      error_message = api.create_with_password(api.implicitSessionToken, "new.wallet", "");
      console.log(error_message);
      assert.equal(error_message.includes("password.size() > 0"), true);

      /*
        Without a password limit (`wallet_manager_impl::create`), we get:

        Aborted(OOM)
        RuntimeError: Aborted(OOM). Build with -sASSERTIONS for more info.
        at abort (file:///src/build_wasm/beekeeper_wasm.mjs:8:5768)
        at abortOnCannotGrowMemory (file:///src/build_wasm/beekeeper_wasm.mjs:8:108604)
        at _emscripten_resize_heap (file:///src/build_wasm/beekeeper_wasm.mjs:8:108707)
      */
      const length = 1e7; // 10M
      const longStr = 'a'.repeat(length);
      error_message = api.create_with_password(api.implicitSessionToken, "new.wallet", longStr);
      console.log(error_message);
      assert.equal(error_message.includes("!password || password->size() < max_password_length"), true);
    }
    {
      api.setAcceptError = true;

      const error_message = api.close(api.implicitSessionToken, "abc");
      console.log(error_message);
      assert.equal(error_message.includes("Wallet not found: abc"), true);
    }
    {
      api.setAcceptError = true;

      let error_message = api.getPublicKeys(api.implicitSessionToken);
      console.log(error_message);
      assert.equal(error_message.includes("You don't have any wallet"), true);

      api.setAcceptError = false;

      const walletNo_9 = 9;
      api.open(api.implicitSessionToken, walletNames[walletNo_9]);

      api.setAcceptError = true;

      error_message = api.getPublicKeys(api.implicitSessionToken);
      console.log(error_message);
      assert.equal(error_message.includes("You don't have any unlocked wallet"), true);

      api.setAcceptError = false;

      const walletNo_4 = 4;
      api.open(api.implicitSessionToken, walletNames[walletNo_4]);
      let wallets = api.listWallets(api.implicitSessionToken);
      requireWalletsIn(wallets, 4, false, 9, false);
      console.log(wallets);

      api.unlock(api.implicitSessionToken, walletNames[walletNo_4]);
      wallets = api.listWallets(api.implicitSessionToken);
      requireWalletsIn(wallets, 4, true, 9, false);
      console.log(wallets);

      api.setAcceptError = false;

      error_message = api.getPublicKeys(api.implicitSessionToken);
      console.log(error_message);
    }
    {
      const walletNo = 8;

      api.setAcceptError = true;

      let error_message = api.lock(api.implicitSessionToken, walletNames[walletNo]);
      console.log(error_message);
      assert.equal(error_message.includes("Wallet not found: w8"), true);

      api.setAcceptError = false;

      error_message = api.open(api.implicitSessionToken, walletNames[walletNo]);
      console.log(error_message);

      api.setAcceptError = true;

      error_message = api.lock(api.implicitSessionToken, walletNames[walletNo]);
      console.log(error_message);
      assert.equal(error_message.includes("Unable to lock a locked wallet"), true);
    }
    {
      const walletNo = 2;

      api.setAcceptError = true;

      let error_message = api.unlock(api.implicitSessionToken, walletNames[walletNo], "");
      console.log(error_message);
      assert.equal(error_message.includes("password.size() > 0"), true);
      
      error_message = api.unlock(api.implicitSessionToken, walletNames[walletNo], "strawberry");
      console.log(error_message);
      assert.equal(error_message.includes("Invalid password for wallet"), true);

      error_message = api.unlock(api.implicitSessionToken, "this_not_wallet", "strawberry");
      console.log(error_message);
      assert.equal(error_message.includes("Unable to open file"), true);

      api.setAcceptError = false;

      error_message = api.unlock(api.implicitSessionToken, walletNames[walletNo]);
      console.log(error_message);

      api.setAcceptError = true;

      error_message = api.unlock(api.implicitSessionToken, walletNames[walletNo]);
      console.log(error_message);
      assert.equal(error_message.includes("Wallet is already unlocked: w2"), true);
    }
    {
      api.setAcceptError = true;
      let error_message = api.importKey(api.implicitSessionToken, "pear", "key");
      console.log(error_message);
      assert.equal(error_message.includes("Wallet not found: pear"), true);

      api.setAcceptError = false;
      const walletNo = 0;
      api.open(api.implicitSessionToken, walletNames[walletNo]);

      api.setAcceptError = true;
      error_message = api.importKey(api.implicitSessionToken, walletNames[walletNo], "key");
      console.log(error_message);
      assert.equal(error_message.includes("Wallet is locked: w0"), true);

      api.setAcceptError = false;
      api.unlock(api.implicitSessionToken, walletNames[walletNo]);

      api.setAcceptError = true;
      error_message = api.importKey(api.implicitSessionToken, walletNames[walletNo], "peach");
      console.log(error_message);
      assert.equal(error_message.includes("Key can't be constructed"), true);

      api.setAcceptError = false;
      api.importKey(api.implicitSessionToken, walletNames[walletNo], keys[0][0]);

      api.setAcceptError = true;
      error_message = api.importKey(api.implicitSessionToken, walletNames[walletNo], keys[0][0]);
      console.log(error_message);
      assert.equal(error_message.includes("Key already in wallet"), true);
    }
    {
      const walletNo = 1;

      api.setAcceptError = true;
      let error_message = api.removeKey(api.implicitSessionToken, walletNames[walletNo], "", "");
      console.log(error_message);
      assert.equal(error_message.includes("Wallet not found: w1"), true);

      api.setAcceptError = false;
      error_message = api.open(api.implicitSessionToken, walletNames[walletNo]);

      api.setAcceptError = true;
      error_message = api.removeKey(api.implicitSessionToken, walletNames[walletNo], "", "");
      console.log(error_message);
      assert.equal(error_message.includes("Wallet is locked: w1"), true);
      
      api.setAcceptError = false;
      error_message = api.unlock(api.implicitSessionToken, walletNames[walletNo]);

      api.setAcceptError = true;
      error_message = api.removeKey(api.implicitSessionToken, walletNames[walletNo], "", "");
      console.log(error_message);
      assert.equal(error_message.includes("password.size() > 0"), true);

      error_message = api.removeKey(api.implicitSessionToken, walletNames[walletNo], "", "xxxxx");
      console.log(error_message);
      assert.equal(error_message.includes("Invalid password for wallet:"), true);

      error_message = api.removeKey(api.implicitSessionToken, walletNames[walletNo], "");
      console.log(error_message);
      assert.equal(error_message.includes("public_key.size() > 0"), true);

      error_message = api.removeKey(api.implicitSessionToken, walletNames[walletNo], "currant");
      console.log(error_message);
      assert.equal(error_message.includes("s == sizeof(data)"), true);

      error_message = api.removeKey(api.implicitSessionToken, walletNames[walletNo], "6Pg5jd1w8rXgGoqvpZXy1tHPdz43itPW6L2AGJuw8kgSAbtsxm");
      console.log(error_message);
      assert.equal(error_message.includes("Key not in wallet"), true);
    }
    {
      api.setAcceptError = true;

      let error_message = api.signDigest(api.implicitSessionToken, "#", "lemon");
      console.log(error_message);
      assert.equal(error_message.includes("Invalid hex character '#'"), true);

      error_message = api.signDigest(api.implicitSessionToken, "abCDe", "lemon");
      console.log(error_message);
      assert.equal(error_message.includes("public_key_size == public_key.size()"), true);

      error_message = api.signDigest(api.implicitSessionToken, "abCDe", "6Pg5jd1w8rXgGoqvpZXy1tHPdz43itPW6L2AGJuw8kgSAbtsxm");
      console.log(error_message);
      assert.equal(error_message.includes("Public key not found in unlocked wallets"), true);
      assert.equal(error_message.includes("6Pg5jd1w8rXgGoqvpZXy1tHPdz43itPW6L2AGJuw8kgSAbtsxm"), true);
    }
    {
      api.setAcceptError = true;

      let error_message = api.signTransaction(api.implicitSessionToken, "+", chainId, "6Pg5jd1w8rXgGoqvpZXy1tHPdz43itPW6L2AGJuw8kgSAbtsxm");
      console.log(error_message);
      assert.equal(error_message.includes("Unexpected char '43' in \"\""), true);

      error_message = api.signTransaction(api.implicitSessionToken, "{}", "$", "6Pg5jd1w8rXgGoqvpZXy1tHPdz43itPW6L2AGJuw8kgSAbtsxm");
      console.log(error_message);
      assert.equal(error_message.includes("Invalid hex character '$'"), true);

      error_message = api.signTransaction(api.implicitSessionToken, "{}", "6542634abcef", "444444444444");
      console.log(error_message);
      assert.equal(error_message.includes("public_key_size == public_key.size()"), true);

      error_message = api.signTransaction(api.implicitSessionToken, "{}", "6542634abcef", "6Pg5jd1w8rXgGoqvpZXy1tHPdz43itPW6L2AGJuw8kgSXbtsxm");
      console.log(error_message);
      assert.equal(error_message.includes("memcmp( (char*)&check, data.data + sizeof(key), sizeof(check) ) == 0"), true);

      error_message = api.signTransaction(api.implicitSessionToken, "{}", "6542634abcef", "6Pg5jd1w8rXgGoqvpZXy1tHPdz43itPW6L2AGJuw8kgSAbtsxm");
      console.log(error_message);
      assert.equal(error_message.includes("Public key not found in unlocked wallets"), true);
      assert.equal(error_message.includes("6Pg5jd1w8rXgGoqvpZXy1tHPdz43itPW6L2AGJuw8kgSAbtsxm"), true);
    }
    {
      api.setAcceptError = true;

      let error_message = api.signBinaryTransaction(api.implicitSessionToken, "+", chainId, "6Pg5jd1w8rXgGoqvpZXy1tHPdz43itPW6L2AGJuw8kgSAbtsxm");
      console.log(error_message);
      assert.equal(error_message.includes("Public key not found in unlocked wallets"), true);
      assert.equal(error_message.includes("6Pg5jd1w8rXgGoqvpZXy1tHPdz43itPW6L2AGJuw8kgSAbtsxm"), true);

      error_message = api.signBinaryTransaction(api.implicitSessionToken, "!@", chainId, "6Pg5jd1w8rXgGoqvpZXy1tHPdz43itPW6L2AGJuw8kgSAbtsxm");
      console.log(error_message);
      assert.equal(error_message.includes("Invalid hex character '!'"), true);
    }
  }
  {
    console.log();
    console.log("**************************************************************************************");
    console.log("An incorrect initialization should block all API calls.");
    console.log("**************************************************************************************");

    {
      const params = new provider.StringList();
      params.push_back("--invalid-param");
      params.push_back("true");
  
      let instance = new provider.beekeeper_api(params)
  
      let error_message = instance.init();
      console.log(error_message)
      assert.equal(error_message.includes("unrecognised option"), true);

      error_message = instance.create_session('xyz');
      console.log(error_message)
      assert.equal(error_message.includes("Initialization failed. API call aborted."), true);
    }
  }

  console.log('##########################################################################################');
  console.log('##                             ALL TESTS PASSED                                         ##');
  console.log('##########################################################################################');
};

my_entrypoint()
  .then(() => process.exit(0))
  .catch(err => {
    console.error(err);
    process.exit(1);
  });
