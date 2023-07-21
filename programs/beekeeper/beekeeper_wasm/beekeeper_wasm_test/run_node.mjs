import { fileURLToPath } from 'url';
import { dirname } from 'path';
import * as path from 'path';

import { strict as assert } from 'node:assert';

import {setTimeout} from 'timers/promises';

const __filename = fileURLToPath(import.meta.url);
const __dirname = dirname(__filename);

import Module from "../../../../build_wasm/beekeeper_wasm.mjs";

const STORAGE_ROOT = path.join(__dirname, "storage_root");

async function perform_StringList_test(provider) {
  const params = new provider.StringList();
  params.push_back("--wallet-dir");
  params.push_back(STORAGE_ROOT + "/.beekeeper");
  params.push_back("--salt");
  params.push_back("avocado");

  var s = params.size();
  console.log("Built params container has a size: " + s);

  assert.equal(s, 4);

  return params;
}

async function create_beekeeper_instance(provider, params) {
  console.log('create beekeeper');
  var instance = new provider.beekeeper_api(params);
  console.log('beekeeper created');

  console.log('init instance');
  var init_result = instance.init();
  console.log('instance initalized with result: ' + init_result);
  var json_result = JSON.parse(init_result);
  assert.equal(json_result.status, true);
  var implicit_session_token = JSON.parse(init_result).token

  return [instance, implicit_session_token];
}

async function create_beekeeper_wallet(provider, beekeeper_instance, session_token, wallet_name, explicit_password) {
  console.log('Attempting to create a wallet: ' + wallet_name + ' using session token: ' + session_token);
  var returned_password = beekeeper_instance.create(session_token, wallet_name, explicit_password)
  console.log('wallet "' + wallet_name + '" created with password: ' + returned_password);
  const json_object = JSON.parse(returned_password);
  return json_object.password;
}

async function import_private_key(provider, beekeeper_instance, session_token, wallet_name, private_key, __expected_public_key) {
  console.log('Importing key ' + private_key + ' to wallet: "' + wallet_name + '"');
  var returned_public_key = beekeeper_instance.import_key(session_token, wallet_name, private_key)
  console.log('public key imported: ' + returned_public_key);

  const public_key = JSON.parse(returned_public_key).public_key;

  assert.equal(public_key, __expected_public_key);

  return public_key;
}

async function test_sign_transaction(beekeeper_instance, session_token, transaction_body, public_key, __sig_digest, __expected_signature) {

  const chain_id = "18dcf0a285365fc58b71f18b3d3fec954aa0c141c44e4e5cb4cf777b9eab274e";

  console.log('Attempting to sign transaction: ' + transaction_body);
  var sig_digest = "390f34297cfcb8fa4b37353431ecbab05b8dc0c9c15fb9ca1a3d510c52177542"
  console.log('************************sign digest************************');
  var s = beekeeper_instance.sign_digest(session_token, public_key, __sig_digest);
  var signature = JSON.parse(s).signature;
  console.log('got signature: ' + signature);

  assert.equal(signature, __expected_signature);

  console.log('************************sign transaction************************');
  var s = beekeeper_instance.sign_transaction(session_token, transaction_body, chain_id, public_key, __sig_digest)
  var signature = JSON.parse(s).signature;
  console.log('got signature: ' + signature);
  assert.equal(signature, __expected_signature);

  return signature;
}

async function perform_transaction_signing_tests(beekeeper_instance, session_token, public_key) {
  console.log('========================SIGNING EXAMPLE 1========================');

  var transaction_body = '{}';
  var sig_digest = "390f34297cfcb8fa4b37353431ecbab05b8dc0c9c15fb9ca1a3d510c52177542"; /// Eliminate precomputed digest - missing API
  await test_sign_transaction(beekeeper_instance, session_token, transaction_body, public_key, sig_digest,
    '1f17cc07f7c769073d39fac3385220b549e261fb33c5f619c5dced7f5b0fe9c0954f2684e703710840b7ea01ad7238b8db1d8a9309d03e93de212f86de38d66f21');

  console.log('========================SIGNING EXAMPLE 2========================');
  /// Second example
  var transaction_body = '{"ref_block_num":95,"ref_block_prefix":4189425605,"expiration":"2023-07-18T08:38:29","operations":[{"type":"transfer_operation","value":{"from":"initminer","to":"alice","amount":{"amount":"666","precision":3,"nai":"@@000000021"},"memo":"memmm"}}],"extensions":[],"signatures":[],"transaction_id":"cc9630cdbc39da1c9b6264df3588c7bedb5762fa","block_num":0,"transaction_num":0}'
  var sig_digest = "614e645c13b351b56d9742b358e3c3da58fa1a6a0036a01d3163c21aa2c8a99c"
  await test_sign_transaction(beekeeper_instance, session_token, transaction_body, public_key, sig_digest,
    '1f69e091fc79b0e8d1812fc662f12076561f9e38ffc212b901ae90fe559f863ad266fe459a8e946cff9bbe7e56ce253bbfab0cccdde944edc1d05161c61ae86340');

}

async function perform_wallet_autolock_test(beekeeper_instance, session_token) {

  console.log('Attempting to set autolock timeout [1s]');
  beekeeper_instance.set_timeout(session_token, 1);

  var start = Date.now();

  console.log('waiting 1.5s...');

  const res = await setTimeout( 1500, 'timed-out');

  const interval = Date.now() - start;

  console.log(`Timer resumed after ${interval} ms`);
  var wallets = beekeeper_instance.list_wallets(session_token);
  console.log(wallets);
  assert.equal(wallets, '{"wallets":[{"name":"wallet_a","unlocked":false},{"name":"wallet_b","unlocked":false}]}');
}

async function my_entrypoint() {
  const provider = await Module();

  console.log("Storage root for testing is: " + STORAGE_ROOT);

  var params = await perform_StringList_test(provider);

  var s = params.size();

  console.log("Returned params has a size: " + s);
  assert.equal(s, 4);

  const [beekeeper_instance, implicit_session_token] = await create_beekeeper_instance(provider, params);

  const implicit_wallet_name = "wallet_created_by_implicit_token"

  var implicit_password = await create_beekeeper_wallet(provider, beekeeper_instance, implicit_session_token, implicit_wallet_name, "");

  const implicit_private_key = "5JkFnXrLM2ap9t3AmAxBJvQHF7xSKtnTrCTginQCkhzU5S7ecPT"
  const implicit_public_key = await import_private_key(provider, beekeeper_instance, implicit_session_token, implicit_wallet_name, implicit_private_key, "5RqVBAVNp5ufMCetQtvLGLJo7unX9nyCBMMrTXRWQ9i1Zzzizh");

  console.log('create session[1]');
  var session_token = beekeeper_instance.create_session("pear")
  console.log('session[1] created with token: ' + session_token);
  session_token = JSON.parse(session_token).token

  console.log('close session[1] with token: ' + session_token);
  beekeeper_instance.close_session(session_token)
  console.log('session[1] closed with token: ' + session_token);

  console.log('create session[2]');
  session_token = beekeeper_instance.create_session("pear")
  console.log('session[2] created with token: ' + session_token);
  session_token = JSON.parse(session_token).token

  const wallet_name = "wallet_a"
  var password_a = await create_beekeeper_wallet(provider, beekeeper_instance, session_token, wallet_name, "cherry");

  var wallet_name_2 = "wallet_b"
  console.log('create wallet: ' + wallet_name_2 + ' with empty password');
  var password_2 = await create_beekeeper_wallet(provider, beekeeper_instance, session_token, wallet_name_2, "");

  const wallet_2_public_key1 = await import_private_key(provider, beekeeper_instance, session_token, wallet_name_2, "5KGKYWMXReJewfj5M29APNMqGEu173DzvHv5TeJAg9SkjUeQV78", "6oR6ckA4TejTWTjatUdbcS98AKETc3rcnQ9dWxmeNiKDzfhBZa");
  const wallet_2_public_key2 = await import_private_key(provider, beekeeper_instance, session_token, wallet_name_2, "5KNbAE7pLwsLbPUkz6kboVpTR24CycqSNHDG95Y8nbQqSqd6tgS", "7j1orEPpWp4bU2SuH46eYXuXkFKEMeJkuXkZVJSaru2zFDGaEH");
  const wallet_2_public_key3 = await import_private_key(provider, beekeeper_instance, session_token, wallet_name_2, "5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n", "6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4");

  var wallets = beekeeper_instance.list_wallets(session_token)
  console.log("Defined wallets: " + wallets);

  assert.equal(wallets, '{"wallets":[{"name":"wallet_a","unlocked":true},{"name":"wallet_b","unlocked":true}]}');

  var public_keys = beekeeper_instance.get_public_keys(session_token)
  console.log("Defined public keys: " + public_keys);

  assert.equal(public_keys, '{"keys":[{"public_key":"6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4"},{"public_key":"6oR6ckA4TejTWTjatUdbcS98AKETc3rcnQ9dWxmeNiKDzfhBZa"},{"public_key":"7j1orEPpWp4bU2SuH46eYXuXkFKEMeJkuXkZVJSaru2zFDGaEH"}]}');

  console.log('Attempting to close wallet: ' + wallet_name_2);
  beekeeper_instance.close(session_token, wallet_name_2)
  var wallets = beekeeper_instance.list_wallets(session_token)
  console.log("Opened wallets: " + wallets);

  assert.equal(wallets, '{"wallets":[{"name":"wallet_a","unlocked":true}]}');

  console.log('Attempting to open wallet: ' + wallet_name_2);
  beekeeper_instance.open(session_token, wallet_name_2)

  var wallets = beekeeper_instance.list_wallets(session_token)
  console.log("Opened wallets: " + wallets);

  assert.equal(wallets, '{"wallets":[{"name":"wallet_a","unlocked":true},{"name":"wallet_b","unlocked":false}]}');

  console.log('Attempting to unlock wallet: ' + wallet_name_2);
  beekeeper_instance.unlock(session_token, wallet_name_2, password_2)

  var wallets = beekeeper_instance.list_wallets(session_token)
  console.log("Opened wallets: " + wallets);

  assert.equal(wallets, '{"wallets":[{"name":"wallet_a","unlocked":true},{"name":"wallet_b","unlocked":true}]}');

  console.log('Attempting to lock wallet: ' + wallet_name_2);
  beekeeper_instance.lock(session_token, wallet_name_2)

  var wallets = beekeeper_instance.list_wallets(session_token)
  console.log("Opened wallets: " + wallets);

  assert.equal(wallets, '{"wallets":[{"name":"wallet_a","unlocked":true},{"name":"wallet_b","unlocked":false}]}');

  beekeeper_instance.lock_all(session_token);

  var wallets = beekeeper_instance.list_wallets(session_token)
  console.log("Opened wallets: " + wallets);

  assert.equal(wallets, '{"wallets":[{"name":"wallet_a","unlocked":false},{"name":"wallet_b","unlocked":false}]}');

  console.log('Attempting to unlock wallet: ' + wallet_name_2);
  beekeeper_instance.unlock(session_token, wallet_name_2, password_2);

  console.log('Attempting to remove key: ' + wallet_2_public_key2 + ' from wallet: ' + wallet_name_2 + ' with pass: ' + password_2);
  beekeeper_instance.remove_key(session_token, wallet_name_2, password_2, wallet_2_public_key2);

  var public_keys = beekeeper_instance.get_public_keys(session_token);
  console.log(public_keys);

  assert.equal(public_keys, '{"keys":[{"public_key":"6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4"},{"public_key":"6oR6ckA4TejTWTjatUdbcS98AKETc3rcnQ9dWxmeNiKDzfhBZa"}]}');

  var info = beekeeper_instance.get_info(session_token)
  console.log(info);

  console.log('Attempting to unlock wallet: ' + wallet_name);
  beekeeper_instance.unlock(session_token, wallet_name, password_a);

  var wallets = beekeeper_instance.list_wallets(session_token);
  console.log(wallets);

  var public_keys = beekeeper_instance.get_public_keys(session_token);
  console.log(public_keys);

  await perform_transaction_signing_tests(beekeeper_instance, session_token, wallet_2_public_key3);

  await perform_wallet_autolock_test(beekeeper_instance, session_token);

  console.log('Attempting to delete beekeeper instance...');
  beekeeper_instance.delete();

  console.log('##########################################################################################');
  console.log('##                             ALL TESTS PASSED                                         ##');
  console.log('##########################################################################################');
}

my_entrypoint()
  .then(() => process.exit(0))
  .catch(err => {
    console.error(err);
    process.exit(1);
  });

