import { BeekeeperApi } from "./api.js";
import { BeekeeperSession } from "./session.js";
import { IBeekeeperUnlockedWallet, IBeekeeperSession, TPublicKey, IBeekeeperWallet, TSignature } from "../interfaces.js";

interface IImportKeyResponse {
  public_key: string;
}

interface IBeekeeperSignature {
  signature: string;
}

interface IBeekeeperKeys {
  keys: Array<{
    public_key: string;
  }>;
}

export class BeekeeperUnlockedWallet implements IBeekeeperUnlockedWallet {
  public constructor(
    private readonly api: BeekeeperApi,
    private readonly session: BeekeeperSession,
    private readonly locked: BeekeeperLockedWallet
  ) {}

  get name(): string {
    return this.locked.name;
  }

  public async lock(): Promise<BeekeeperLockedWallet> {
    this.api.extract(this.api.api.lock(this.session.token, this.locked.name) as string);
    this.locked.unlocked = undefined;

    await this.api.fs.sync();

    return this.locked;
  }

  public async importKey(wifKey: string): Promise<TPublicKey> {
    const { public_key } = this.api.extract(this.api.api.import_key(this.session.token, this.locked.name, wifKey) as string) as IImportKeyResponse;

    await this.api.fs.sync();

    return public_key;
  }

  public async removeKey(password: string, publicKey: TPublicKey): Promise<void> {
    this.api.extract(this.api.api.remove_key(this.session.token, this.locked.name, password, publicKey) as string);

    await this.api.fs.sync();
  }

  public async signDigest(publicKey: string, sigDigest: string): Promise<TSignature> {
    const result = this.api.extract(this.api.api.sign_digest(this.session.token, sigDigest, publicKey) as string) as IBeekeeperSignature;

    await this.api.fs.sync();

    return result.signature;
  }

  public async getPublicKeys(): Promise<TPublicKey[]> {
    const result = this.api.extract(this.api.api.get_public_keys(this.session.token) as string) as IBeekeeperKeys;

    await this.api.fs.sync();

    return result.keys.map(value => value.public_key);
  }

  public close(): Promise<IBeekeeperSession> {
    return this.locked.close();
  }
}

export class BeekeeperLockedWallet implements IBeekeeperWallet {
  public unlocked: BeekeeperUnlockedWallet | undefined = undefined;

  public constructor(
    private readonly api: BeekeeperApi,
    private readonly session: BeekeeperSession,
    public readonly name: string
  ) {}

  public async unlock(password: string): Promise<IBeekeeperUnlockedWallet> {
    this.api.extract(this.api.api.unlock(this.session.token, this.name, password) as string);
    this.unlocked = new BeekeeperUnlockedWallet(this.api, this.session, this);

    await this.api.fs.sync();

    return this.unlocked;
  }

  public async close(): Promise<IBeekeeperSession> {
    if(typeof this.unlocked !== 'undefined')
      await this.unlocked.lock();

    await this.session.closeWallet(this.name);

    return this.session;
  }
}
