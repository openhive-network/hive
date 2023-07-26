import { strict as assert } from 'node:assert';
import path from 'path';

import { setTimeout } from 'timers/promises';
import { fileURLToPath } from 'url';

import Module from '../../../../build_wasm/beekeeper_wasm.mjs';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const wallet_names = ["w0","w1","w2","w3","w4","w5","w6","w7","w8"];
const passwords = new Map();

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

const STORAGE_ROOT = path.join(__dirname, 'storage_root');

const perform_StringList_test = provider => {
  const params = new provider.StringList();
  params.push_back('--wallet-dir');
  params.push_back(`${STORAGE_ROOT}/.beekeeper`);
  params.push_back('--salt');
  params.push_back('avocado');

  const s = params.size();
  console.log(`Built params container has a size: ${s}`);

  assert.equal(s, 4);

  return params;
};

const create_beekeeper_instance = (provider, params) => {
  console.log('create beekeeper');
  const instance = new provider.beekeeper_api(params);
  console.log('beekeeper created');

  console.log('init instance');
  const init_result = instance.init();
  console.log(`instance initalized with result: ${init_result}`);
  const { status } = JSON.parse(init_result);
  assert.equal(status, true);
  const { token } = JSON.parse(init_result);

  return [ instance, token ];
};

const create_beekeeper_wallet = (beekeeper_instance, session_token, wallet_number, explicit_password) => {
  console.log(`Attempting to create a wallet: ${wallet_names[wallet_number]} using session token: ${session_token}`);

  const returned_password = beekeeper_instance.create(session_token, wallet_names[wallet_number], explicit_password);
  console.log(`wallet "${wallet_names[wallet_number]}" created with password: ${returned_password}`);
  const { password } = JSON.parse(returned_password);

  passwords.set(wallet_number, password);
};

const create_session = (beekeeper_instance, salt) => {
  console.log(`Attempting to create a session using salt: ${salt}`);

  let returned_token = beekeeper_instance.create_session(salt);
  console.log(`session created with token: ${returned_token}`);
  const { token } = JSON.parse(returned_token);

  return token;
};

const close_session = (beekeeper_instance, token) => {
  console.log(`close session with token: ${token}`);
  beekeeper_instance.close_session(token);
  console.log(`session closed with token: ${token}`);
};

const import_private_key = (beekeeper_instance, session_token, wallet_name_number, key_number) => {
  console.log(`Importing key ${keys[key_number][0]} to wallet: "${wallet_names[wallet_name_number]}"`);
  const returned_public_key = beekeeper_instance.import_key(session_token, wallet_names[wallet_name_number], keys[key_number][0]);
  console.log(`public key imported: ${returned_public_key}`);

  const { public_key } = JSON.parse(returned_public_key);

  assert.equal(public_key, keys[key_number][1]);
};

const remove_private_key = (beekeeper_instance, session_token, wallet_name_number, key_number) => {
  console.log(`Removing key ${keys[key_number][1]} from wallet: "${wallet_names[wallet_name_number]}"`);
  beekeeper_instance.remove_key(session_token, wallet_names[wallet_name_number], passwords.get(wallet_name_number), keys[key_number][1]);
  console.log(`Key ${keys[key_number][1]} was removed from wallet: "${wallet_names[wallet_name_number]}"`);
};

const test_sign_transaction = (beekeeper_instance, session_token, transaction_body, binary_transaction_body, public_key, sig_digest, expected_signature) => {
  const chain_id = '18dcf0a285365fc58b71f18b3d3fec954aa0c141c44e4e5cb4cf777b9eab274e';

  console.log(`Attempting to sign transaction: ${transaction_body}`);
  console.log('sign digest...');
  let s = beekeeper_instance.sign_digest(session_token, public_key, sig_digest);
  let { signature } = JSON.parse(s);
  console.log(`got signature: ${signature}`);

  assert.equal(signature, expected_signature);

  console.log('sign transaction...');
  s = beekeeper_instance.sign_transaction(session_token, transaction_body, chain_id, public_key);
  ({ signature } = JSON.parse(s));
  console.log(`got signature: ${signature}`);
  assert.equal(signature, expected_signature);

  console.log('sign binary transaction...');
  s = beekeeper_instance.sign_binary_transaction(session_token, binary_transaction_body, chain_id, public_key);
  ({ signature } = JSON.parse(s));
  console.log(`got signature: ${signature}`);
  assert.equal(signature, expected_signature);

  return signature;
};

const perform_transaction_signing_tests = (beekeeper_instance, session_token, public_key) => {
  console.log('sign empty transaction...');
  let transaction_body = '{}';
  let binary_transaction_body = '000000000000000000000000';
  let sig_digest = '390f34297cfcb8fa4b37353431ecbab05b8dc0c9c15fb9ca1a3d510c52177542';
  test_sign_transaction(beekeeper_instance, session_token, transaction_body, binary_transaction_body, public_key, sig_digest, '1f17cc07f7c769073d39fac3385220b549e261fb33c5f619c5dced7f5b0fe9c0954f2684e703710840b7ea01ad7238b8db1d8a9309d03e93de212f86de38d66f21');

  console.log('sign not empty transaction...');
  transaction_body = '{"ref_block_num":95,"ref_block_prefix":4189425605,"expiration":"2023-07-18T08:38:29","operations":[{"type":"transfer_operation","value":{"from":"initminer","to":"alice","amount":{"amount":"666","precision":3,"nai":"@@000000021"},"memo":"memmm"}}],"extensions":[],"signatures":[],"transaction_id":"cc9630cdbc39da1c9b6264df3588c7bedb5762fa","block_num":0,"transaction_num":0}';
  binary_transaction_body = '5f00c58fb5f9854fb664010209696e69746d696e657205616c6963659a020000000000002320bcbe056d656d6d6d00';
  sig_digest = '614e645c13b351b56d9742b358e3c3da58fa1a6a0036a01d3163c21aa2c8a99c';
  test_sign_transaction(beekeeper_instance, session_token, transaction_body, binary_transaction_body, public_key, sig_digest, '1f69e091fc79b0e8d1812fc662f12076561f9e38ffc212b901ae90fe559f863ad266fe459a8e946cff9bbe7e56ce253bbfab0cccdde944edc1d05161c61ae86340');
};

const list_wallets = (beekeeper_instance, session_token) => {
  let wallets = beekeeper_instance.list_wallets(session_token);
  console.log(`wallets: ${wallets}`);
  return wallets;
};

const get_public_keys = (beekeeper_instance, session_token) => {
  let keys = beekeeper_instance.get_public_keys(session_token);
  console.log(`public keys: ${keys}`);
  return keys;
};

const open = (beekeeper_instance, session_token, wallet_name) => {
  console.log(`attempting to open wallet: ${wallet_name}`);
  beekeeper_instance.open(session_token, wallet_name);
  console.log(`wallet: ${wallet_name} was opened`);
};

const close = (beekeeper_instance, session_token, wallet_name) => {
  console.log(`attempting to close wallet: ${wallet_name}`);
  beekeeper_instance.close(session_token, wallet_name);
  console.log(`wallet: ${wallet_name} was closed`);
};

const unlock = (beekeeper_instance, session_token, wallet_name_number) => {
  console.log(`attempting to unlock wallet: ${wallet_names[wallet_name_number]}`);
  beekeeper_instance.unlock(session_token, wallet_names[wallet_name_number], passwords.get(wallet_name_number));
  console.log(`wallet: ${wallet_names[wallet_name_number]} was unlocked`);
};

const lock = (beekeeper_instance, session_token, wallet_name_number) => {
  console.log(`attempting to lock wallet: ${wallet_names[wallet_name_number]}`);
  beekeeper_instance.lock(session_token, wallet_names[wallet_name_number]);
  console.log(`wallet: ${wallet_names[wallet_name_number]} was locked`);
};

const perform_wallet_autolock_test = async(beekeeper_instance, session_token) => {
  console.log('Attempting to set autolock timeout [1s]');
  beekeeper_instance.set_timeout(session_token, 1);

  const start = Date.now();

  console.log('waiting 1.5s...');

  await setTimeout( 1500, 'timed-out');

  const interval = Date.now() - start;

  console.log(`Timer resumed after ${interval} ms`);
  const wallets = list_wallets(beekeeper_instance, session_token);
  assert.equal(wallets, '{"wallets":[{"name":"w1","unlocked":false},{"name":"w2","unlocked":false}]}');
};

const my_entrypoint = async() => {
  const provider = await Module();

  console.log(`Storage root for testing is: ${STORAGE_ROOT}`);

  const params = perform_StringList_test(provider);

  {
    console.log("")
    console.log("**************************************************************************************")
    console.log("Basic tests which execute all(16) endpoints")
    console.log("**************************************************************************************")

    const [ beekeeper_instance, implicit_session_token ] = create_beekeeper_instance(provider, params);

    create_beekeeper_wallet(beekeeper_instance, implicit_session_token, 0, '');

    import_private_key(beekeeper_instance, implicit_session_token, 0, 0);

    let session_token = create_session(beekeeper_instance, 'pear');

    close_session(beekeeper_instance, session_token)

    session_token = create_session(beekeeper_instance, 'pear');

    create_beekeeper_wallet(beekeeper_instance, session_token, 1, 'cherry');

    create_beekeeper_wallet(beekeeper_instance, session_token, 2, '');

    import_private_key(beekeeper_instance, session_token, 2, 1);
    import_private_key(beekeeper_instance, session_token, 2, 2);
    import_private_key(beekeeper_instance, session_token, 2, 3);

    let wallets = list_wallets(beekeeper_instance, session_token);
    assert.equal(wallets, '{"wallets":[{"name":"w1","unlocked":true},{"name":"w2","unlocked":true}]}');

    let public_keys = get_public_keys(beekeeper_instance, session_token);
    assert.equal(public_keys, '{"keys":[{"public_key":"6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4"},{"public_key":"6oR6ckA4TejTWTjatUdbcS98AKETc3rcnQ9dWxmeNiKDzfhBZa"},{"public_key":"7j1orEPpWp4bU2SuH46eYXuXkFKEMeJkuXkZVJSaru2zFDGaEH"}]}');

    close(beekeeper_instance, session_token, wallet_names[2]);
    
    wallets = list_wallets(beekeeper_instance, session_token);

    assert.equal(wallets, '{"wallets":[{"name":"w1","unlocked":true}]}');

    open(beekeeper_instance, session_token, wallet_names[2]);

    wallets = list_wallets(beekeeper_instance, session_token);
    assert.equal(wallets, '{"wallets":[{"name":"w1","unlocked":true},{"name":"w2","unlocked":false}]}');

    unlock(beekeeper_instance, session_token, 2);

    wallets = list_wallets(beekeeper_instance, session_token);
    assert.equal(wallets, '{"wallets":[{"name":"w1","unlocked":true},{"name":"w2","unlocked":true}]}');

    lock(beekeeper_instance, session_token, 2);

    wallets = list_wallets(beekeeper_instance, session_token);

    assert.equal(wallets, '{"wallets":[{"name":"w1","unlocked":true},{"name":"w2","unlocked":false}]}');

    beekeeper_instance.lock_all(session_token);

    wallets = list_wallets(beekeeper_instance, session_token);

    assert.equal(wallets, '{"wallets":[{"name":"w1","unlocked":false},{"name":"w2","unlocked":false}]}');

    unlock(beekeeper_instance, session_token, 2);

    remove_private_key(beekeeper_instance, session_token, 2, 2);

    public_keys = get_public_keys(beekeeper_instance, session_token);
    assert.equal(public_keys, '{"keys":[{"public_key":"6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4"},{"public_key":"6oR6ckA4TejTWTjatUdbcS98AKETc3rcnQ9dWxmeNiKDzfhBZa"}]}');

    console.log(beekeeper_instance.get_info(session_token));

    unlock(beekeeper_instance, session_token, 1);

    wallets = list_wallets(beekeeper_instance, session_token);

    public_keys = get_public_keys(beekeeper_instance, session_token);

    perform_transaction_signing_tests(beekeeper_instance, session_token, keys[3][1]);

    await perform_wallet_autolock_test(beekeeper_instance, session_token);

    console.log('Attempting to delete beekeeper instance...');
    beekeeper_instance.delete();
  }

  {
    console.log("")
    console.log("**************************************************************************************")
    console.log("Trying to open beekeeper instance again so as to find out if data created in previous test are permamently saved")
    console.log("**************************************************************************************")
    const [ beekeeper_instance, session_token ] = create_beekeeper_instance(provider, params);

    beekeeper_instance.list_wallets(session_token);

    create_beekeeper_wallet(beekeeper_instance, session_token, 3, '');

    list_wallets(beekeeper_instance, session_token);

    unlock(beekeeper_instance, session_token, 2);

    list_wallets(beekeeper_instance, session_token);

    import_private_key(beekeeper_instance, session_token, 3, 9);

    let public_keys = get_public_keys(beekeeper_instance, session_token);
    assert.equal(public_keys, '{"keys":[{"public_key":"6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4"},{"public_key":"6oR6ckA4TejTWTjatUdbcS98AKETc3rcnQ9dWxmeNiKDzfhBZa"},{"public_key":"8mmxXz5BfQc2NJfqhiPkbgcyJm4EvWEr2UAUdr56gEWSN9ZnA5"}]}');
  }
  {
    console.log("")
    console.log("**************************************************************************************")
    console.log("Create 3 wallets and add to every wallet 3 the same keys. Every key should be displayed only once")
    console.log("**************************************************************************************")
    const [ beekeeper_instance, session_token ] = create_beekeeper_instance(provider, params);

    create_beekeeper_wallet(beekeeper_instance, session_token, 4, '');
    create_beekeeper_wallet(beekeeper_instance, session_token, 5, '');
    create_beekeeper_wallet(beekeeper_instance, session_token, 6, '');

    for(let wallet_number = 4; wallet_number <= 6; wallet_number++)
    {
      for(let key_number = 7; key_number <= 9; key_number++)
      {
        import_private_key(beekeeper_instance, session_token, wallet_number, key_number);
      }
    }
    let public_keys = get_public_keys(beekeeper_instance, session_token);
    assert.equal(public_keys, '{"keys":[{"public_key":"6a34GANY5LD8deYvvfySSWGd7sPahgVNYoFPapngMUD27pWb45"},{"public_key":"8FDsHdPkHbY8fuUkVLyAmrnKMvj6DddLopi3YJ51dVqsG9vZa4"},{"public_key":"8mmxXz5BfQc2NJfqhiPkbgcyJm4EvWEr2UAUdr56gEWSN9ZnA5"}]}');
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
