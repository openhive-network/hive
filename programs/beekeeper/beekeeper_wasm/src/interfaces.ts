export type TPublicKey = string;
export type TSignature = string;

export interface IWallet {
  /**
   * Ensures that this wallet is locked, then closes it
   *
   * @returns {Promise<IBeekeeperSession>} Beekeeper session owning the closed wallet
   *
   * @throws {import("../errors").BeekeeperError} on any beekeeper API-related error (error parsing response, invalid input, timeout error, fs sync error etc.)
   */
  close(): Promise<IBeekeeperSession>;

  /**
   * Name of this wallet
   *
   * @type {string}
   * @readonly
   */
  readonly name: string;
};

export interface IBeekeeperInfo {
  /**
   * Current server's time
   *
   * Note: Time is in format: YYYY-MM-DDTHH:mm:ss
   *
   * @type {string}
   */
  now: string;

  /**
   * Time when wallets will be automatically closed
   *
   * Note: Time is in format: YYYY-MM-DDTHH:mm:ss
   *
   * @type {string}
   */
  timeout_time: string;
}

export interface IBeekeeperOptions {
  /**
   * The path of the wallet files (absolute path or relative to application data dir). Parent of the `.beekeeper` directory
   *
   * @type {string}
   * @default "/storage_root"
   */
  storageRoot: string;

  /**
   * Whether logs can be written. By default logs are enabled
   *
   * @default true
   */
  enableLogs: boolean;

  /**
   * Timeout for unlocked wallet in seconds (default 900 - 15 minutes).
   * Wallets will automatically lock after specified number of seconds of inactivity.
   * Activity is defined as any wallet command e.g. list-wallets
   *
   * @type {number}
   * @default 900
   */
  unlockTimeout: number;
}

export interface IBeekeeperUnlockedWallet extends IWallet {
  /**
   * Locks the current wallet
   *
   * @returns {Promise<IBeekeeperWallet>} Locked beekeeper wallet
   *
   * @throws {import("../errors").BeekeeperError} on any beekeeper API-related error (error parsing response, invalid input, timeout error, fs sync error etc.)
   */
  lock(): Promise<IBeekeeperWallet>;

  /**
   * Imports given private key to this wallet
   *
   * @param {string} wifKey private key in WIF format to import
   *
   * @returns {Promise<TPublicKey>} Public key generated from the imported private key
   *
   * @throws {import("../errors").BeekeeperError} on any beekeeper API-related error (error parsing response, invalid input, timeout error, fs sync error etc.)
   */
  importKey(wifKey: string): Promise<TPublicKey>;

  /**
   * Removes given key from this wallet
   *
   * @param {string} password password to the wallet
   * @param {TPublicKey} publicKey public key in WIF format to match the private key in the wallet to remove
   *
   * @throws {import("../errors").BeekeeperError} on any beekeeper API-related error (error parsing response, invalid input, timeout error, fs sync error etc.)
   */
  removeKey(password: string, publicKey: TPublicKey): Promise<void>;

  /**
   * Signs a transaction by signing a digest of the transaction
   *
   * @param {TPublicKey} publicKey public key in WIF format to match the private key in the wallet. It will be used to sign the provided data
   * @param {string} sigDigest digest of a transaction in hex format
   *
   * @returns {Promise<TSignature>} signed data in hex format
   *
   * @throws {import("../errors").BeekeeperError} on any beekeeper API-related error (error parsing response, invalid input, timeout error, fs sync error etc.)
   */
  signDigest(publicKey: TPublicKey, sigDigest: string): Promise<TSignature>;

  /**
   * Lists all of the public keys
   *
   * @returns {Promise<TPublicKey[]>} a set of all keys for all unlocked wallets
   *
   * @throws {import("../errors").BeekeeperError} on any beekeeper API-related error (error parsing response, invalid input, timeout error, fs sync error etc.)
   */
  getPublicKeys(): Promise<TPublicKey[]>;
}

export interface IBeekeeperWallet extends IWallet {
  /**
   * Unlocks this wallet
   *
   * @param {string} password password to the wallet
   *
   * @returns {Promise<IBeekeeperUnlockedWallet>} Unlocked Beekeeper wallet
   *
   * @throws {import("../errors").BeekeeperError} on any beekeeper API-related error (error parsing response, invalid input, timeout error, fs sync error etc.)
   */
  unlock(password: string): Promise<IBeekeeperUnlockedWallet>;

  /**
   * Indicates if the wallet is unlocked. If the wallet is locked, this property will be undefined. IBeekeeperUnlockedWallet type otherwise
   *
   * @type {?IBeekeeperUnlockedWallet}
   * @readonly
   */
  readonly unlocked?: IBeekeeperUnlockedWallet;
};

export interface IWalletCreated {
  /**
   * Unlocked, ready to use wallet
   *
   * @type {IBeekeeperUnlockedWallet}
   */
  wallet: IBeekeeperUnlockedWallet;

  /**
   * Password used for unlocking your wallet
   *
   * @type {string}
   */
  password: string;
}

export interface IBeekeeperSession {
  /**
   * Retrieves the current session info
   *
   * @returns {Promise<IBeekeeperInfo>} Current session information
   *
   * @throws {import("../errors").BeekeeperError} on any beekeeper API-related error (error parsing response, invalid input, timeout error, fs sync error etc.)
   */
  getInfo(): Promise<IBeekeeperInfo>;

  /**
   * Lists all of the opened wallets
   *
   * @returns {Promise<Array<IBeekeeperWallet>>} array of opened Beekeeper wallets (either unlocked or locked)
   *
   * @throws {import("../errors").BeekeeperError} on any beekeeper API-related error (error parsing response, invalid input, timeout error, fs sync error etc.)
   */
  listWallets(): Promise<Array<IBeekeeperWallet>>;

  /**
   * Creates a new Beekeeper wallet object owned by this session
   *
   * @param {string} name name of wallet
   * @param {?string} password password used for creation of a wallet. Not required and in this case a password is automatically generated and returned
   *
   * @returns {Promise<IWalletCreated>} the created unlocked Beekeeper wallet object
   *
   * @throws {import("../errors").BeekeeperError} on any beekeeper API-related error (error parsing response, invalid input, timeout error, fs sync error etc.)
   */
  createWallet(name: string, password?: string): Promise<IWalletCreated>;

  /**
   * Locks all of the unlocked wallets owned by this session
   *
   * @returns {Promise<Array<IBeekeeperWallet>>} array of the locked Beekeeper wallets
   *
   * @throws {import("../errors").BeekeeperError} on any beekeeper API-related error (error parsing response, invalid input, timeout error, fs sync error etc.)
   */
  lockAll(): Promise<Array<IBeekeeperWallet>>;

  /**
   * Locks all of the unlocked wallets, closes them, closes this session and makes it unusable
   *
   * @returns {Promise<IBeekeeperInstance>} Beekeeper instance owning the closed session
   *
   * @throws {import("../errors").BeekeeperError} on any beekeeper API-related error (error parsing response, invalid input, timeout error, fs sync error etc.)
   */
  close(): Promise<IBeekeeperInstance>;
}

export interface IBeekeeperInstance {
  /**
   * Creation of a session
   *
   * @param {string} salt a salt used for creation of a token
   *
   * @returns {Promise<IBeekeeperSession>} a beekeeper session created explicitly. It can be used for further work for example: creating/closing wallets, importing keys, signing transactions etc.
   *
   * @throws {import("../errors").BeekeeperError} on any beekeeper API-related error (error parsing response, invalid input, timeout error, fs sync error etc.)
   */
  createSession(salt: string): Promise<IBeekeeperSession>;

  /**
   * Locks all of the unlocked wallets, closes them, closes opened sessions and deletes the current Beekeeper API instance making it unusable
   *
   * @throws {import("../errors").BeekeeperError} on any beekeeper API-related error (error parsing response, invalid input, timeout error, fs sync error etc.)
   */
  delete(): Promise<void>;
}
