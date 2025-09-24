import { BeekeeperError } from "./errors.js";
import { BeekeeperApi } from "./api.js";
import { IBeekeeperInfo, IBeekeeperInstance, IBeekeeperSession, IBeekeeperWallet, IWalletCreated } from "./interfaces.js";
import { BeekeeperLockedWallet, BeekeeperUnlockedWallet } from "./wallet.js";
import { safeWasmCall } from './util/wasm_error.js';

interface IBeekeeperWalletPassword {
  password: string;
}

interface IBeekeeperHasWallet {
  exists: boolean;
}

interface IBeekeeperWallets {
  wallets: Array<{
    name: string;
    unlocked: boolean;
  }>;
}

interface IBeekeeperSessionInfo {
  now: string;
  timeout_time: string;
}

export class BeekeeperSession implements IBeekeeperSession {
  public constructor(
    private readonly api: BeekeeperApi,
    public readonly token: string
  ) {}

  public readonly wallets: Map<string, BeekeeperLockedWallet> = new Map();

  public getInfo(): IBeekeeperInfo {
    const result = this.api.extract(safeWasmCall(() => this.api.api.get_info(this.token) as string, "session info retrieval")) as IBeekeeperSessionInfo;

    return {
      now: new Date(`${result.now}Z`),
      timeoutTime: new Date(`${result.timeout_time}Z`)
    };
  }

  public listWallets(): Array<IBeekeeperWallet> {
    const result = this.api.extract(safeWasmCall(() => this.api.api.list_wallets(this.token) as string, "listing wallets")) as IBeekeeperWallets;

    const wallets: IBeekeeperWallet[] = [];

    for(const value of result.wallets) {
      const wallet = this.openWallet(value.name);

      if(!value.unlocked)
        (wallet as BeekeeperLockedWallet).unlocked = undefined;

      wallets.push(wallet);
    }

    return wallets;
  }

  public hasWallet(name: string): boolean {
    const result = this.api.extract(safeWasmCall(() => this.api.api.has_wallet(this.token, name) as string, `checking if wallet '${name}' exists`)) as IBeekeeperHasWallet;

    return result.exists;
  }

  public async createWallet(name: string, password: string | undefined, isTemporary: boolean = false): Promise<IWalletCreated> {
    if (!isTemporary && this.api.fs === undefined)
      throw new BeekeeperError(
        "Trying to create persistent wallet without a filesystem (consider disabling the 'inMemory' Beekeeper option or setting 'isTemporary' function argument to true)."
      );

    if(typeof password === 'string')
      this.api.extract(safeWasmCall(() => this.api.api.create(this.token, name, password as string, isTemporary) as string, `${isTemporary ? 'temporary ' : ''}wallet '${name}' creation`));
    else {
      const result = this.api.extract(safeWasmCall(() => this.api.api.create(this.token, name) as string, `wallet '${name} creation'`)) as IBeekeeperWalletPassword;
      ({ password } = result);
    }

    await this.api.fs?.sync();

    const wallet = new BeekeeperLockedWallet(this.api, this, name, isTemporary);
    wallet.unlocked = new BeekeeperUnlockedWallet(this.api, this, wallet);

    this.wallets.set(name, wallet);

    return {
      wallet: wallet.unlocked,
      password
    };
  }

  public openWallet(name: string): IBeekeeperWallet {
    if(this.wallets.has(name))
      return this.wallets.get(name) as IBeekeeperWallet;

    this.api.extract(safeWasmCall(() => this.api.api.open(this.token, name) as string, `wallet '${name}' opening`));
    const wallet = new BeekeeperLockedWallet(this.api, this, name, false);

    this.wallets.set(name, wallet);

    return wallet;
  }

  public closeWallet(name: string): void {
    if(!this.wallets.delete(name))
      throw new BeekeeperError(`This Beekeeper API session is not the owner of wallet identified by name: "${name}"`);

    this.api.extract(safeWasmCall(() => this.api.api.close(this.token, name) as string, `wallet '${name}' closing`));
  }

  public lockAll(): Array<IBeekeeperWallet> {
    const wallets = Array.from(this.wallets.values());
    for(const wallet of wallets)
      if(typeof wallet.unlocked !== 'undefined')
        wallet.unlocked.lock();

    return wallets;
  }

  public close(): IBeekeeperInstance {
    for(const wallet of this.wallets.values())
      wallet.close();

    this.api.closeSession(this.token);

    return this.api;
  }
}
