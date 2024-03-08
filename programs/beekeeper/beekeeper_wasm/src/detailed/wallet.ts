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

interface IEncryptData {
  encrypted_content: string;
}

interface IDecryptData {
  decrypted_content: string;
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

  public lock(): BeekeeperLockedWallet {
    this.api.extract(this.api.api.lock(this.session.token, this.locked.name) as string);
    this.locked.unlocked = undefined;

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

  public signDigest(publicKey: string, sigDigest: string): TSignature {
    const result = this.api.extract(this.api.api.sign_digest(this.session.token, sigDigest, publicKey) as string) as IBeekeeperSignature;

    return result.signature;
  }

  public getPublicKeys(): TPublicKey[] {
    const result = this.api.extract(this.api.api.get_public_keys(this.session.token) as string) as IBeekeeperKeys;

    return result.keys.map(value => value.public_key);
  }

  public encryptData(content: string, key: TPublicKey, anotherKey: TPublicKey): string {
    const result = this.api.extract(this.api.api.encrypt_data(this.session.token, key, anotherKey || key, this.locked.name, content)) as IEncryptData;

    return result.encrypted_content;
  }

  public decryptData(content: string, key: TPublicKey, anotherKey?: TPublicKey): string {
    const result = this.api.extract(this.api.api.decrypt_data(this.session.token, key, anotherKey || key, this.locked.name, content)) as IDecryptData;

    return result.decrypted_content;
  }

  public close(): IBeekeeperSession {
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

  public unlock(password: string): IBeekeeperUnlockedWallet {
    this.api.extract(this.api.api.unlock(this.session.token, this.name, password) as string);
    this.unlocked = new BeekeeperUnlockedWallet(this.api, this.session, this);

    return this.unlocked;
  }

  public close(): IBeekeeperSession {
    if(typeof this.unlocked !== 'undefined')
      this.unlocked.lock();

    this.session.closeWallet(this.name);

    return this.session;
  }
}
