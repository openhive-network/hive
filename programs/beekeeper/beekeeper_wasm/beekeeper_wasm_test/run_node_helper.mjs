import { strict as assert } from 'node:assert';

export default class BeekeeperInstanceHelper {
  // Private properties:

  #instance = undefined;
  #implicitSessionToken = undefined;

  #sessions = new Set();
  #passwords = new Map();

  // Getters for private properties:

  get implicitSessionToken() {
    return this.#implicitSessionToken;
  }

  get instance() {
    return this.#instance;
  }

  // Private functions:

  #setPassword(sessionToken, walletName, password) {
    this.#passwords.set(`${sessionToken}|${walletName}`, password);
  }

  #getPassword(sessionToken, walletName) {
    const pass = this.#passwords.get(`${sessionToken}|${walletName}`);
    assert.ok(pass);

    return pass;
  }

  #extract(json) {
    const parsed = JSON.parse(json);

    assert.ok( parsed.hasOwnProperty('result'), "Response resulted with error" );

    return JSON.parse(parsed.result);
  }

  #parseStringList(provider, ...options) {
    const params = new provider.StringList();
    for(const option of options)
      params.push_back(option);

    const s = params.size();
    assert.equal(s, options.length);

    return params;
  }

  /// Prepares beekeeper instance helper for given profider
  static for(provider) {
    return BeekeeperInstanceHelper.bind(undefined, provider);
  }

  constructor(provider, ...options) {
    const params = this.#parseStringList(provider, ...options);
    this.#instance = new provider.beekeeper_api(params);

    const initResult = this.instance.init();
    this.#implicitSessionToken = this.#extract(initResult).token;

    this.#sessions.add(this.implicitSessionToken);
  }

  // Public helper methods:

  closeAllSessions() {
    for(const token of this.#sessions)
      this.closeSession(token);
  }

  createSession(salt) {
    const returnedValue = this.instance.create_session(salt);

    const value = this.#extract(returnedValue);

    this.#sessions.add(value.token);

    return value.token;
  }

  closeSession(token) {
    const returnedValue = this.instance.close_session(token);
    this.#extract(returnedValue);

    this.#sessions.delete(token);
  }

  create(sessionToken, walletName, explicitPassword) {
    const returnedValue = this.instance.create(sessionToken, walletName, explicitPassword);

    const value = this.#extract(returnedValue);
    this.#setPassword(sessionToken, walletName, value.password);

    return value.password;
  }

  importKey(sessionToken, walletName, key) {
    const returnedValue = this.instance.import_key(sessionToken, walletName, key);

    const value = this.#extract(returnedValue);

    return value.public_key;
  };

  removeKey(sessionToken, walletName, key) {
    const pass = this.#getPassword(sessionToken, walletName);
    const returnedValue = beekeeper_instance.remove_key(sessionToken, walletName, pass, key);

    const value = this.#extract(returnedValue);

    return value.public_key;
  }
}


const limit_display = (str, max) => str.length > max ? (str.slice(0, max - 3) + '...') : str;

const sign_digest = (beekeeper_instance, session_token, sig_digest, public_key, expected_signature) => {
  console.log(`signing digest ${limit_display(sig_digest)}`);
  const returned_value = beekeeper_instance.sign_digest(session_token, public_key, sig_digest);

  const value = extract_json(returned_value);

  assert.equal(value.signature, expected_signature);
  console.log(`signing digest ${returned_value} passed`);

  return value.signature;
};

const sign_binary_transaction = (beekeeper_instance, session_token, binary_transaction_body, chain_id, public_key, expected_signature) => {
  let binary_transaction_body_display = binary_transaction_body;
  if( binary_transaction_body_display.length > 100 )
  binary_transaction_body_display = binary_transaction_body_display.slice(0, 97) + '...';

  let a = "";
  if( binary_transaction_body.length > 100 )
  {
    _binary_transaction_body = binary_transaction_body.substring(0, 100);
    _binary_transaction_body += "...";
  }
  else
    _binary_transaction_body = binary_transaction_body;

  console.log(`signing binary transaction ${_binary_transaction_body}`);
  let returned_value = beekeeper_instance.sign_binary_transaction(session_token, binary_transaction_body, chain_id, public_key);

  let [ _value, _response_status ] = extract_json(returned_value);

  if( _response_status )
  {
    console.log(`signing binary transaction ${_binary_transaction_body} passed`);
    _response_status = _value.signature == expected_signature;
  }
  else
    console.log(`signing binary transaction ${_binary_transaction_body} failed`);

  return [_value.signature, _response_status];
};

const sign_transaction = (beekeeper_instance, session_token, transaction_body, chain_id, public_key, expected_signature) => {
  console.log(`signing transaction ${transaction_body}`);
  let returned_value = beekeeper_instance.sign_transaction(session_token, transaction_body, chain_id, public_key);

  let [ _value, _response_status ] = extract_json(returned_value);

  if( _response_status )
  {
    console.log(`signing transaction ${transaction_body} passed`);
    _response_status = _value.signature == expected_signature;
  }
  else
    console.log(`signing transaction ${transaction_body} failed`);

  return [_value.signature, _response_status];
};

const sign_transaction_using_3_methods = (beekeeper_instance, session_token, idx) => {
  console.log('sign transaction using 3 methods...');

  let _data = sign_data[idx];

  let _value = 0;
  let _response_status = 0;

  [_value, _response_status] = sign_digest(beekeeper_instance, session_token, _data['sig_digest'], _data['public_key'], _data['expected_signature']);
  if( _response_status )
  {
    [_value, _response_status] = sign_binary_transaction(beekeeper_instance, session_token, _data['binary_transaction_body'], chain_id, _data['public_key'], _data['expected_signature']);
    if( _response_status )
    {
      [_value, _response_status] = sign_transaction(beekeeper_instance, session_token, _data['transaction_body'], chain_id, _data['public_key'], _data['expected_signature']);
    }
  }
  return [_value, _response_status];
};

const list_wallets = (beekeeper_instance, session_token) => {
  console.log(`attempting to retrieve wallets`);
  let returned_value = beekeeper_instance.list_wallets(session_token);
  const [_wallets, _response_status] = extract_json(returned_value);

  if( _response_status )
    console.log(`retrieving wallets ${JSON.stringify(_wallets)} passed`);
  else
    console.log(`retrieving wallets failed`);

  return [JSON.stringify(_wallets), _response_status];
};

const get_public_keys = (beekeeper_instance, session_token) => {
  console.log(`attempting to retrieve public keys`);
  let returned_value = beekeeper_instance.get_public_keys(session_token);
  const [_keys, _response_status] = extract_json(returned_value);

  if( _response_status )
    console.log(`retrieving public keys ${JSON.stringify(_keys)} passed`);
  else
    console.log(`retrieving public keys failed`);

  return [JSON.stringify(_keys), _response_status];
};

const get_info = (beekeeper_instance, session_token) => {
  console.log(`attempting to retrieve info`);
  let returned_value = beekeeper_instance.get_info(session_token);
  const [_info, _response_status] = extract_json(returned_value);

  if( _response_status )
    console.log(`retrieving info ${JSON.stringify(_info)} passed`);
  else
    console.log(`retrieving info failed`);

  return [Date.parse(_info['now']), Date.parse(_info['timeout_time']), _response_status];
};

const open = (beekeeper_instance, session_token, wallet_name_number) => {
  console.log(`attempting to open wallet: ${wallet_names[wallet_name_number]}`);
  let returned_value = beekeeper_instance.open(session_token, wallet_names[wallet_name_number]);
  const value = extract_json(returned_value);

  if( _response_status )
    console.log(`wallet: ${wallet_names[wallet_name_number]} was opened`);
  else
    console.log(`opening wallet: ${wallet_names[wallet_name_number]} failed`);

  return [_value, _response_status];
};

const close = (beekeeper_instance, session_token, wallet_name_number) => {
  console.log(`attempting to close wallet: ${wallet_names[wallet_name_number]}`);
  let returned_value = beekeeper_instance.close(session_token, wallet_names[wallet_name_number]);
  const value = extract_json(returned_value);

  if( _response_status )
    console.log(`closing wallet: ${wallet_names[wallet_name_number]} passed`);
  else
    console.log(`closing wallet: ${wallet_names[wallet_name_number]} failed`);

  return [_value, _response_status];
};

const unlock = (beekeeper_instance, session_token, wallet_name_number) => {
  console.log(`attempting to unlock wallet: ${wallet_names[wallet_name_number]} using a session_token/password: ${session_token}/${passwords.get(wallet_name_number)}`);
  let returned_value = beekeeper_instance.unlock(session_token, wallet_names[wallet_name_number], passwords.get(wallet_name_number));
  const value = extract_json(returned_value);

  if( _response_status )
    console.log(`unlocking wallet: ${wallet_names[wallet_name_number]} passed`);
  else
    console.log(`unlocking wallet: ${wallet_names[wallet_name_number]} failed`);

  return [_value, _response_status];
};

const lock = (beekeeper_instance, session_token, wallet_name_number) => {
  console.log(`attempting to lock wallet: ${wallet_names[wallet_name_number]}`);
  let returned_value = beekeeper_instance.lock(session_token, wallet_names[wallet_name_number]);

  const value = extract_json(returned_value);

  if( _response_status )
    console.log(`unlocking wallet: ${wallet_names[wallet_name_number]} passed`);
  else
    console.log(`unlocking wallet: ${wallet_names[wallet_name_number]} failed`);

  return [_value, _response_status];
};

const delete_instance = (beekeeper_instance) => {
  console.log('attempting to delete beekeeper instance...');
  beekeeper_instance.delete();
  console.log('beekeeper instance was deleted...');
};

const set_timeout = (beekeeper_instance, session_token, seconds) => {
  console.log(`attempting to set timeout ${seconds}[s] using a session_token: ${session_token}`);
  let returned_value = beekeeper_instance.set_timeout(session_token, seconds);
  const value = extract_json(returned_value);

  if( _response_status )
    console.log(`setting timeout with a token ${session_token} passed`);
  else
    console.log(`setting timeout with a token ${session_token} failed`);

  return [_value, _response_status];
};

const lock_all = (beekeeper_instance, session_token) => {
  console.log(`attempting to lock all wallets using a session_token: ${session_token}`);
  let returned_value = beekeeper_instance.lock_all(session_token);
  const value = extract_json(returned_value);

  if( _response_status )
    console.log(`locking all wallets ${session_token} passed`);
  else
    console.log(`locking all wallets ${session_token} failed`);

  return [_value, _response_status];
};

const perform_wallet_autolock_test = async(beekeeper_instance, session_token) => {
  console.log('Attempting to set autolock timeout [1s]');
  beekeeper_instance.set_timeout(session_token, 1);

  const start = Date.now();

  console.log('waiting 1.5s...');

  await setTimeout( 1500, 'timed-out');

  const interval = Date.now() - start;

  console.log(`Timer resumed after ${interval} ms`);
  let [wallets, _response_status_wallets] = list_wallets(beekeeper_instance, session_token);
  //although WASM beekeeper automatic locking is disabled, calling API endpoint triggers locks for every wallet
  assert.equal(_response_status_wallets, true);
  assert.equal(wallets, '{"wallets":[{"name":"w1","unlocked":false},{"name":"w2","unlocked":false}]}');
};