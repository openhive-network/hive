import { strict as assert } from 'node:assert';
import path from 'path';

import { setTimeout } from 'timers/promises';
import { fileURLToPath } from 'url';

import Module from '../../../../build_wasm/beekeeper_wasm.mjs';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

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

const create_beekeeper_wallet = (beekeeper_instance, session_token, wallet_name, explicit_password) => {
  console.log(`Attempting to create a wallet: ${wallet_name} using session token: ${session_token}`);

  const returned_password = beekeeper_instance.create(session_token, wallet_name, explicit_password);
  console.log(`wallet "${wallet_name}" created with password: ${returned_password}`);
  const { password } = JSON.parse(returned_password);

  return password;
};

const import_private_key = (beekeeper_instance, session_token, wallet_name, private_key, expected_public_key) => {
  console.log(`Importing key ${private_key} to wallet: "${wallet_name}"`);
  const returned_public_key = beekeeper_instance.import_key(session_token, wallet_name, private_key);
  console.log(`public key imported: ${returned_public_key}`);

  const { public_key } = JSON.parse(returned_public_key);

  assert.equal(public_key, expected_public_key);

  return public_key;
};

const test_sign_transaction = (beekeeper_instance, session_token, transaction_body, binary_transaction_body, public_key, sig_digest, expected_signature) => {
  const chain_id = '18dcf0a285365fc58b71f18b3d3fec954aa0c141c44e4e5cb4cf777b9eab274e';

  console.log(`Attempting to sign transaction: ${transaction_body}`);
  console.log('************************sign digest************************');
  let s = beekeeper_instance.sign_digest(session_token, public_key, sig_digest);
  let { signature } = JSON.parse(s);
  console.log(`got signature: ${signature}`);

  assert.equal(signature, expected_signature);

  console.log('************************sign transaction************************');
  s = beekeeper_instance.sign_transaction(session_token, transaction_body, chain_id, public_key);
  ({ signature } = JSON.parse(s));
  console.log(`got signature: ${signature}`);
  assert.equal(signature, expected_signature);

  console.log('************************sign binary transaction************************');
  s = beekeeper_instance.sign_binary_transaction(session_token, binary_transaction_body, chain_id, public_key);
  ({ signature } = JSON.parse(s));
  console.log(`got signature: ${signature}`);
  assert.equal(signature, expected_signature);

  return signature;
};

const perform_transaction_signing_tests = (beekeeper_instance, session_token, public_key) => {
  console.log('========================SIGNING EXAMPLE 1========================');

  let transaction_body = '{}';
  let binary_transaction_body = '000000000000000000000000';
  let sig_digest = '390f34297cfcb8fa4b37353431ecbab05b8dc0c9c15fb9ca1a3d510c52177542'; // Eliminate precomputed digest - missing API
  test_sign_transaction(beekeeper_instance, session_token, transaction_body, binary_transaction_body, public_key, sig_digest, '1f17cc07f7c769073d39fac3385220b549e261fb33c5f619c5dced7f5b0fe9c0954f2684e703710840b7ea01ad7238b8db1d8a9309d03e93de212f86de38d66f21');

  console.log('========================SIGNING EXAMPLE 2========================');
  // Second example
  transaction_body = '{"ref_block_num":95,"ref_block_prefix":4189425605,"expiration":"2023-07-18T08:38:29","operations":[{"type":"transfer_operation","value":{"from":"initminer","to":"alice","amount":{"amount":"666","precision":3,"nai":"@@000000021"},"memo":"memmm"}}],"extensions":[],"signatures":[],"transaction_id":"cc9630cdbc39da1c9b6264df3588c7bedb5762fa","block_num":0,"transaction_num":0}';
  binary_transaction_body = '5f00c58fb5f9854fb664010209696e69746d696e657205616c6963659a020000000000002320bcbe056d656d6d6d00';
  sig_digest = '614e645c13b351b56d9742b358e3c3da58fa1a6a0036a01d3163c21aa2c8a99c';
  test_sign_transaction(beekeeper_instance, session_token, transaction_body, binary_transaction_body, public_key, sig_digest, '1f69e091fc79b0e8d1812fc662f12076561f9e38ffc212b901ae90fe559f863ad266fe459a8e946cff9bbe7e56ce253bbfab0cccdde944edc1d05161c61ae86340');
};

const perform_wallet_autolock_test = async(beekeeper_instance, session_token) => {
  console.log('Attempting to set autolock timeout [1s]');
  beekeeper_instance.set_timeout(session_token, 1);

  const start = Date.now();

  console.log('waiting 1.5s...');

  await setTimeout( 1500, 'timed-out');

  const interval = Date.now() - start;

  console.log(`Timer resumed after ${interval} ms`);
  const wallets = beekeeper_instance.list_wallets(session_token);
  console.log(wallets);
  assert.equal(wallets, '{"wallets":[{"name":"wallet_a","unlocked":false},{"name":"wallet_b","unlocked":false}]}');
};

const my_entrypoint = async() => {
  const provider = await Module();

  console.log(`Storage root for testing is: ${STORAGE_ROOT}`);

  const params = perform_StringList_test(provider);

  const [ beekeeper_instance, implicit_session_token ] = create_beekeeper_instance(provider, params);

  const implicit_wallet_name = 'wallet_created_by_implicit_token';

  create_beekeeper_wallet(beekeeper_instance, implicit_session_token, implicit_wallet_name, '');

  const implicit_private_key = '5JkFnXrLM2ap9t3AmAxBJvQHF7xSKtnTrCTginQCkhzU5S7ecPT';
  import_private_key(beekeeper_instance, implicit_session_token, implicit_wallet_name, implicit_private_key, '5RqVBAVNp5ufMCetQtvLGLJo7unX9nyCBMMrTXRWQ9i1Zzzizh');

  console.log('create session[1]');
  let session_token = beekeeper_instance.create_session('pear');
  console.log(`session[1] created with token: ${session_token}`);
  ({ token: session_token } = JSON.parse(session_token));

  console.log(`close session[1] with token: ${session_token}`);
  beekeeper_instance.close_session(session_token);
  console.log(`session[1] closed with token: ${session_token}`);

  console.log('create session[2]');
  session_token = beekeeper_instance.create_session('pear');
  console.log(`session[2] created with token: ${session_token}`);
  ({ token: session_token } = JSON.parse(session_token));;

  const wallet_name = 'wallet_a';
  const password_a = create_beekeeper_wallet(beekeeper_instance, session_token, wallet_name, 'cherry');

  const wallet_name_2 = 'wallet_b';
  console.log(`create wallet: ${wallet_name_2} with empty password`);
  const password_2 = create_beekeeper_wallet(beekeeper_instance, session_token, wallet_name_2, '');

  import_private_key(beekeeper_instance, session_token, wallet_name_2, '5KGKYWMXReJewfj5M29APNMqGEu173DzvHv5TeJAg9SkjUeQV78', '6oR6ckA4TejTWTjatUdbcS98AKETc3rcnQ9dWxmeNiKDzfhBZa');
  const wallet_2_public_key2 = import_private_key(beekeeper_instance, session_token, wallet_name_2, '5KNbAE7pLwsLbPUkz6kboVpTR24CycqSNHDG95Y8nbQqSqd6tgS', '7j1orEPpWp4bU2SuH46eYXuXkFKEMeJkuXkZVJSaru2zFDGaEH');
  const wallet_2_public_key3 = import_private_key(beekeeper_instance, session_token, wallet_name_2, '5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n', '6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4');

  let wallets = beekeeper_instance.list_wallets(session_token);
  console.log(`Defined wallets: ${wallets}`);

  assert.equal(wallets, '{"wallets":[{"name":"wallet_a","unlocked":true},{"name":"wallet_b","unlocked":true}]}');

  let public_keys = beekeeper_instance.get_public_keys(session_token);
  console.log(`Defined public keys: ${public_keys}`);

  assert.equal(public_keys, '{"keys":[{"public_key":"6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4"},{"public_key":"6oR6ckA4TejTWTjatUdbcS98AKETc3rcnQ9dWxmeNiKDzfhBZa"},{"public_key":"7j1orEPpWp4bU2SuH46eYXuXkFKEMeJkuXkZVJSaru2zFDGaEH"}]}');

  console.log(`Attempting to close wallet: ${wallet_name_2}`);
  beekeeper_instance.close(session_token, wallet_name_2);
  wallets = beekeeper_instance.list_wallets(session_token);
  console.log(`Opened wallets: ${wallets}`);

  assert.equal(wallets, '{"wallets":[{"name":"wallet_a","unlocked":true}]}');

  console.log(`Attempting to open wallet: ${wallet_name_2}`);
  beekeeper_instance.open(session_token, wallet_name_2);

  wallets = beekeeper_instance.list_wallets(session_token);
  console.log(`Opened wallets: ${wallets}`);

  assert.equal(wallets, '{"wallets":[{"name":"wallet_a","unlocked":true},{"name":"wallet_b","unlocked":false}]}');

  console.log(`Attempting to unlock wallet: ${wallet_name_2}`);
  beekeeper_instance.unlock(session_token, wallet_name_2, password_2);

  wallets = beekeeper_instance.list_wallets(session_token);
  console.log(`Opened wallets: ${wallets}`);

  assert.equal(wallets, '{"wallets":[{"name":"wallet_a","unlocked":true},{"name":"wallet_b","unlocked":true}]}');

  console.log(`Attempting to lock wallet: ${wallet_name_2}`);
  beekeeper_instance.lock(session_token, wallet_name_2);

  wallets = beekeeper_instance.list_wallets(session_token);
  console.log(`Opened wallets: ${wallets}`);

  assert.equal(wallets, '{"wallets":[{"name":"wallet_a","unlocked":true},{"name":"wallet_b","unlocked":false}]}');

  beekeeper_instance.lock_all(session_token);

  wallets = beekeeper_instance.list_wallets(session_token);
  console.log(`Opened wallets: ${wallets}`);

  assert.equal(wallets, '{"wallets":[{"name":"wallet_a","unlocked":false},{"name":"wallet_b","unlocked":false}]}');

  console.log(`Attempting to unlock wallet: ${wallet_name_2}`);
  beekeeper_instance.unlock(session_token, wallet_name_2, password_2);

  console.log(`Attempting to remove key: ${wallet_2_public_key2} from wallet: ${wallet_name_2} with pass: ${password_2}`);
  beekeeper_instance.remove_key(session_token, wallet_name_2, password_2, wallet_2_public_key2);

  public_keys = beekeeper_instance.get_public_keys(session_token);
  console.log(public_keys);

  assert.equal(public_keys, '{"keys":[{"public_key":"6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4"},{"public_key":"6oR6ckA4TejTWTjatUdbcS98AKETc3rcnQ9dWxmeNiKDzfhBZa"}]}');

  const info = beekeeper_instance.get_info(session_token);
  console.log(info);

  console.log(`Attempting to unlock wallet: ${wallet_name}`);
  beekeeper_instance.unlock(session_token, wallet_name, password_a);

  wallets = beekeeper_instance.list_wallets(session_token);
  console.log(wallets);

  public_keys = beekeeper_instance.get_public_keys(session_token);
  console.log(public_keys);

  perform_transaction_signing_tests(beekeeper_instance, session_token, wallet_2_public_key3);

  await perform_wallet_autolock_test(beekeeper_instance, session_token);

  console.log('Attempting to delete beekeeper instance...');
  beekeeper_instance.delete();

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
