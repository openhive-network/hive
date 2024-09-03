import { BeekeeperError } from "../errors.js";
import { BeekeeperApi } from "./api.js";
import { IBeekeeperInfo, IBeekeeperInstance, IBeekeeperSession, IBeekeeperWallet, IWalletCreated } from "../interfaces.js";
import { BeekeeperLockedWallet, BeekeeperUnlockedWallet } from "./wallet.js";

interface IBeekeeperWalletPassword {
  password: string;
}

interface IBeekeeperWallets {
  wallets: Array<{
    name: string;
    unlocked: boolean;
  }>;
}

export class BeekeeperSession implements IBeekeeperSession {
  public constructor(
    private readonly api: BeekeeperApi,
    public readonly token: string
  ) {}

  public readonly wallets: Map<string, BeekeeperLockedWallet> = new Map();

  public getInfo(): IBeekeeperInfo {
    const result = this.api.extract(this.api.api.get_info(this.token) as string) as IBeekeeperInfo;

    return result;
  }

  public listWallets(): Array<IBeekeeperWallet> {
    const result = this.api.extract(this.api.api.list_wallets(this.token) as string) as IBeekeeperWallets;

    const wallets: IBeekeeperWallet[] = [];

    for(const value of result.wallets) {
      const wallet = this.openWallet(value.name);

      if(!value.unlocked)
        (wallet as BeekeeperLockedWallet).unlocked = undefined;

      wallets.push(wallet);
    }

    return wallets;
  }

  public async createWallet(name: string, password: string | undefined, isTemporary: boolean = false): Promise<IWalletCreated> {
    if(typeof password === 'string')
      this.api.extract(this.api.api.create(this.token, name, password, isTemporary) as string);
    else {
      const result = this.api.extract(this.api.api.create(this.token, name) as string) as IBeekeeperWalletPassword;
      ({ password } = result);
    }

    await this.api.fs.sync();

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

    this.api.extract(this.api.api.open(this.token, name) as string);
    const wallet = new BeekeeperLockedWallet(this.api, this, name, false);

    this.wallets.set(name, wallet);

    return wallet;
  }

  public closeWallet(name: string): void {
    if(!this.wallets.delete(name))
      throw new BeekeeperError(`This Beekeeper API session is not the owner of wallet identified by name: "${name}"`);

    this.api.extract(this.api.api.close(this.token, name) as string);
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
