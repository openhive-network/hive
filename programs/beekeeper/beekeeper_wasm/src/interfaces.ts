export type TPublicKey = string;
export type TSignature = string;

export interface IWallet {
  close(): Promise<IBeekeeperSession>;
  readonly name: string;
};

export interface IBeekeeperInfo {
  now: string;
  timeout_time: string;
}

export interface IBeekeeperOptions {
  storageRoot: string;
  enableLogs: boolean;
  unlockTimeout: number;
}

export interface IBeekeeperUnlockedWallet extends IWallet {
  lock(): Promise<IBeekeeperWallet>;
  importKey(wifKey: string): Promise<TPublicKey>;
  removeKey(password: string, publicKey: TPublicKey): Promise<void>;
  signDigest(publicKey: TPublicKey, sigDigest: string): Promise<TSignature>;
  getPublicKeys(): Promise<TPublicKey[]>;
}

export interface IBeekeeperWallet extends IWallet {
  unlock(password: string): Promise<IBeekeeperUnlockedWallet>;
  readonly unlocked?: IBeekeeperUnlockedWallet;
};

export interface IWalletCreated {
  wallet: IBeekeeperUnlockedWallet;
  password: string;
}

export interface IBeekeeperSession {
  getInfo(): Promise<IBeekeeperInfo>;
  listWallets(): Promise<Array<IBeekeeperWallet>>;
  createWallet(name: string, password?: string): Promise<IWalletCreated>;
  lockAll(): Promise<Array<IBeekeeperWallet>>;
  close(): Promise<IBeekeeperInstance>;
}

export interface IBeekeeperInstance {
  createSession(salt: string): Promise<IBeekeeperSession>;
  delete(): Promise<void>;
}
