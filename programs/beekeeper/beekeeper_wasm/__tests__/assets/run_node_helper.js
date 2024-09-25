export class ExtractError extends Error {
  constructor(parsed) {
    super(`Response resulted with error: "${JSON.stringify(parsed)}"`);
    this.name = 'ExtractError';
    this.parsed = parsed;
  }
}

export class BeekeeperInstanceHelper {
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
    if(!pass)
      throw new Error("Wallet does not exist");

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
    if(s !== options.length)
      throw new Error("Invalid params size");

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
    this.#version = this.#extract(initResult).version;
    this.#implicitSessionToken = this.createSessionWithoutSalt();
  }

  // Public helper methods:

  createSession(salt) {
    const returnedValue = this.instance.create_session(salt);

    const value = this.#extract(returnedValue);

    return value.token;
  }

  createSessionWithoutSalt() {
    const returnedValue = this.instance.create_session();

    const value = this.#extract(returnedValue);

    return value.token;
  }

  closeSession(token) {
    const returnedValue = this.instance.close_session(token);
    return this.#extract(returnedValue);
  }

  hasMatchingPrivateKey(token, walletName, publicKey) {
    const returnedValue = this.instance.has_matching_private_key(token, walletName, publicKey);
    const value = this.#extract(returnedValue);

    return value.exists;
  }

  hasWallet(token, walletName) {
    const returnedValue = this.instance.has_wallet(token, walletName);
    const value = this.#extract(returnedValue);

    return value.exists;
  }

  create(sessionToken, walletName) {
    const returnedValue = this.instance.create(sessionToken, walletName);

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

  create_with_password(sessionToken, walletName, explicitPassword) {
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

  create_temporary_wallet_with_password(sessionToken, walletName, explicitPassword, isTemporary) {
    const returnedValue = this.instance.create(sessionToken, walletName, explicitPassword, isTemporary);

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

    if( this.#acceptError )
    {
      return this.#extract(returnedValue);
    }
    else
    {
      const value = this.#extract(returnedValue);

      return value.public_key;
    }
  };

  importKeys(sessionToken, walletName, keys) {
    const returnedValue = this.instance.import_keys(sessionToken, walletName, keys);

    if( this.#acceptError )
    {
      return this.#extract(returnedValue);
    }
    else
    {
      const value = this.#extract(returnedValue);

      return value.public_key;
    }
  };

  encryptData(sessionToken, fromPublicKey, toPublicKey, walletName, content) {
    const returnedValue = this.instance.encrypt_data(sessionToken, fromPublicKey, toPublicKey, walletName, content);

    if( this.#acceptError )
    {
      return this.#extract(returnedValue);
    }
    else
    {
      const value = this.#extract(returnedValue);

      return value.encrypted_content;
    }
  };

  decryptData(sessionToken, fromPublicKey, toPublicKey, walletName, encryptedContent) {
    const returnedValue = this.instance.decrypt_data(sessionToken, fromPublicKey, toPublicKey, walletName, encryptedContent);

    if( this.#acceptError )
    {
      return this.#extract(returnedValue);
    }
    else
    {
      const value = this.#extract(returnedValue);

      return value.decrypted_content;
    }
  };

  /**
   * @param {string} sessionToken
   * @param {string} walletName
   * @param {string} key
   * @param {null|string} explicitPassword
   */
  removeKey(sessionToken, walletName, key) {
    const returnedValue = this.instance.remove_key(sessionToken, walletName, key);

    return this.#extract(returnedValue);
  }

  signDigest(sessionToken, sigDigest, publicKey) {
    const returnedValue = this.instance.sign_digest(sessionToken, sigDigest, publicKey);

    if( this.#acceptError )
    {
      return this.#extract(returnedValue);
    }
    else
    {
      const value = this.#extract(returnedValue);

      return value.signature;
    }
  }

  listWallets(sessionToken) {
    const returnedValue = this.instance.list_wallets(sessionToken);

    return this.#extract(returnedValue);
  }

  getPublicKeys(sessionToken) {
    const returnedValue = this.instance.get_public_keys(sessionToken);

    return this.#extract(returnedValue);
  }

  getInfo(sessionToken) {
    const returnedValue = this.instance.get_info(sessionToken);

    return this.#extract(returnedValue);
  }

  open(sessionToken, walletName) {
    const returnedValue = this.instance.open(sessionToken, walletName);

    return this.#extract(returnedValue);
  }

  close(sessionToken, walletName) {
    const returnedValue = this.instance.close(sessionToken, walletName);

    return this.#extract(returnedValue);
  }

  /**
   * @param {string} sessionToken
   * @param {string} walletName
   * @param {string | null} explicitPassword
   */
  unlock(sessionToken, walletName, explicitPassword = null) {
    const pass = ( explicitPassword == null ) ? BeekeeperInstanceHelper.#getPassword(walletName) : explicitPassword;
    const returnedValue = this.instance.unlock(sessionToken, walletName, pass);

    return this.#extract(returnedValue);
  }

  lock(sessionToken, walletName) {
    const returnedValue = this.instance.lock(sessionToken, walletName);

    return this.#extract(returnedValue);
  }

  lockAll(sessionToken) {
    const returnedValue = this.instance.lock_all(sessionToken);

    return this.#extract(returnedValue);
  }

  deleteInstance() {
    this.instance.delete();
  }

  setTimeout(sessionToken, seconds) {
    let returnedValue = this.instance.set_timeout(sessionToken, seconds);

    return this.#extract(returnedValue);
  }
}
