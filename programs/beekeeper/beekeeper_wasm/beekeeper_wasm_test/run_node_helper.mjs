import { strict as assert } from 'node:assert';

export class ExtractError extends Error {
  constructor(parsed) {
    super("Response resulted with error");
    this.name = 'ExtractError';
    this.parsed = parsed;
  }
}

export default class BeekeeperInstanceHelper {
  // Private properties:

  #instance = undefined;
  #implicitSessionToken = undefined;
  #version = undefined;

  #acceptError = false;

  static #passwords = new Map();

  // Getters for private properties:

  get implicitSessionToken() {
    return this.#implicitSessionToken;
  }

  get instance() {
    return this.#instance;
  }

  get version() {
    return this.#version;
  }

  /**
   * @param {(arg0: boolean) => void} acceptError
   */
  set setAcceptError(acceptError) {
    this.#acceptError = acceptError;
  }

  // Private functions:

  static #setPassword(walletName, password) {
    BeekeeperInstanceHelper.#passwords.set(walletName, password);
  }

  static #getPassword(walletName) {
    const pass = BeekeeperInstanceHelper.#passwords.get(walletName);
    assert.ok(pass, "Wallet does not exist");

    return pass;
  }

  #extract(json) {
    const parsed = JSON.parse(json);

    if( this.#acceptError )
    {
      if( !parsed.hasOwnProperty('error') )
        throw new ExtractError(parsed);
  
      return JSON.parse(parsed.error);
    }
    else
    {
      if( !parsed.hasOwnProperty('result') )
        throw new ExtractError(parsed);
  
      return JSON.parse(parsed.result);
    }
  }

  #parseStringList(provider, options) {
    const params = new provider.StringList();
    for(const option of options)
      params.push_back(option);

    const s = params.size();
    assert.equal(s, options.length);

    return params;
  }

  /**
   * Prepares beekeeper instance helper for given profider
   * @returns {BeekeeperInstanceHelper}
   */
  static for(provider) {
    return BeekeeperInstanceHelper.bind(undefined, provider);
  }

  constructor(provider, options) {
    const params = this.#parseStringList(provider, options);
    this.#instance = new provider.beekeeper_api(params);

    const initResult = this.instance.init();
    this.#implicitSessionToken = this.#extract(initResult).token;
    this.#version = this.#extract(initResult).version;
  }

  // Public helper methods:

  createSession(salt) {
    const returnedValue = this.instance.create_session(salt);

    const value = this.#extract(returnedValue);

    return value.token;
  }

  closeSession(token) {
    const returnedValue = this.instance.close_session(token);
    this.#extract(returnedValue);
  }

  create(sessionToken, walletName, explicitPassword) {
    const returnedValue = this.instance.create(sessionToken, walletName, explicitPassword);

    if( this.#acceptError )
    {
      return this.#extract(returnedValue);
    }
    else
    {
      const value = this.#extract(returnedValue);
      BeekeeperInstanceHelper.#setPassword(walletName, value.password);

      return value.password;
    }
  }

  importKey(sessionToken, walletName, key) {
    const returnedValue = this.instance.import_key(sessionToken, walletName, key);

    const value = this.#extract(returnedValue);

    return value.public_key;
  };

  removeKey(sessionToken, walletName, key, explicitPassword = undefined) {
    const pass = explicitPassword || BeekeeperInstanceHelper.#getPassword(walletName);
    const returnedValue = this.instance.remove_key(sessionToken, walletName, pass, key);

    this.#extract(returnedValue);
  }

  signDigest(sessionToken, sigDigest, publicKey) {
    const returnedValue = this.instance.sign_digest(sessionToken, publicKey, sigDigest);

    const value = this.#extract(returnedValue);

    return value.signature;
  }

  signBinaryTransaction(sessionToken, binaryTransactionBody, chainId, publicKey) {
    const returnedValue = this.instance.sign_binary_transaction(sessionToken, binaryTransactionBody, chainId, publicKey);

    const value = this.#extract(returnedValue);

    return value.signature;
  }

  signTransaction(sessionToken, transactionBody, chainId, publicKey) {
    const returnedValue = this.instance.sign_transaction(sessionToken, transactionBody, chainId, publicKey);

    const value = this.#extract(returnedValue);

    return value.signature;
  }

  listWallets(sessionToken) {
    const returnedValue = this.instance.list_wallets(sessionToken);

    const wallets = this.#extract(returnedValue);

    return wallets;
  }

  getPublicKeys(sessionToken) {
    const returnedValue = this.instance.get_public_keys(sessionToken);

    const keys = this.#extract(returnedValue);

    return keys;
  }

  getInfo(sessionToken) {
    const returnedValue = this.instance.get_info(sessionToken);

    const info = this.#extract(returnedValue);

    return info;
  }

  open(sessionToken, walletName) {
    const returnedValue = this.instance.open(sessionToken, walletName);

    return this.#extract(returnedValue);
  }

  close(sessionToken, walletName) {
    const returnedValue = this.instance.close(sessionToken, walletName);

    return this.#extract(returnedValue);
  }

  unlock(sessionToken, walletName, explicitPassword = undefined) {
    const pass = explicitPassword || BeekeeperInstanceHelper.#getPassword(walletName);
    const returnedValue = this.instance.unlock(sessionToken, walletName, pass);

    this.#extract(returnedValue);
  }

  lock(sessionToken, walletName) {
    const returnedValue = this.instance.lock(sessionToken, walletName);

    this.#extract(returnedValue);
  }

  lockAll(sessionToken) {
    const returnedValue = this.instance.lock_all(sessionToken);

    this.#extract(returnedValue);
  }

  deleteInstance() {
    this.instance.delete();
  }

  setTimeout(sessionToken, seconds) {
    let returnedValue = this.instance.set_timeout(sessionToken, seconds);

    this.#extract(returnedValue);
  }
}
