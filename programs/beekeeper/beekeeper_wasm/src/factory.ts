import Beekeeper from "./beekeeper.js";
import type { BeekeeperModule, beekeeper_api } from "./beekeeper.js";
import { BeekeeperError } from "./errors.js";

const requiresSyncFs = [
  "close", "close_session", "create", "create_session", "import_key",
  "lock", "lock_all", "open", "remove_key", "set_timeout", "unlock"
] as const;
const doesNotRequireSyncFs = [
  "init", "list_wallets", "get_public_keys", "sign_digest",
  "sign_binary_transaction", "sign_transaction", "get_info"
] as const;

type Callable = {
  (...args: any[]): any;
  };

type DirectMethodGen<C extends Callable> = (...args: Parameters<C>) => ReturnType<C> extends void ? void : object;
type SyncMethodGen<C extends Callable> = (...args: Parameters<C>) => Promise<ReturnType<C> extends void ? void : object>;

type BeekeeperSyncApi = typeof requiresSyncFs[number];

/// Omit init method from final interface since it should be called only internally by factory
type BeekeeperApi = Omit<{
  [method in keyof beekeeper_api]: beekeeper_api[method] extends Function ?
      method extends BeekeeperSyncApi ?
        SyncMethodGen<beekeeper_api[method]>
        :
        DirectMethodGen<beekeeper_api[method]>
    : never;
}, "init">;

type beekeeper_fs = BeekeeperModule['FS'];

interface BeekeeperOptions {
  storage_root: string;
  enable_logs: boolean;
}

class BeekeeperProxy implements ProxyHandler<BeekeeperApi> {
  public constructor(
    private readonly fs: beekeeper_fs
  ) {}

  private sync(): Promise<void> {
    return new Promise((resolve, reject) => {
      this.fs.syncfs((err?: unknown) => {
        if (err) reject(err);

        resolve(undefined);
      });
    });
  }

  public static initFS(beekeeperProvider: BeekeeperModule, walletDir: string): Promise<beekeeper_fs> {
    const fs: beekeeper_fs = beekeeperProvider.FS;
    fs.mkdir(walletDir);
    fs.mount(fs.filesystems.IDBFS, {}, walletDir);

    return new Promise((resolve, reject) => {
      fs.syncfs(true, (err?: unknown) => {
        if (err) reject(err);

        resolve(fs);
      });
    });
  }

  public extract(json: string) {
    try {
      const parsed = JSON.parse(json);

      if(parsed.hasOwnProperty('error'))
        throw new BeekeeperError(`Beekeeper API error: "${String(parsed.error)}"`);

      if( !parsed.hasOwnProperty('result') )
        throw new BeekeeperError(`Beekeeper response does not have contain the result: "${json}"`);

      return JSON.parse(parsed.result);
    } catch(error) {
      if(!(error instanceof BeekeeperError))
        throw new BeekeeperError(`Could not extract the result from the beekeeper response: "${String(error)}"`);
    }
  }

  public get(target: BeekeeperApi, prop: keyof BeekeeperApi, _receiver: any) {
    if((requiresSyncFs as unknown as Array<keyof BeekeeperApi>).includes(prop))
      return async(...args: any[]) => {
        const data = target[prop]( ...args );
        await this.sync();

        if(typeof data === 'string')
          return this.extract(data);
      };
    else if((doesNotRequireSyncFs as unknown as Array<keyof BeekeeperApi>).includes(prop))
      return (...args: any[]) => {
        const data = target[prop](...args);

        if(typeof data === 'string')
          return this.extract(data);
      };

    return target[prop];
  }
}

const createBeekeeper = async (
  { storage_root, enable_logs }: BeekeeperOptions = {
    storage_root: "/storage_root",
    enable_logs: true,
  }
): Promise<BeekeeperApi> => {
  const beekeeperProvider = await Beekeeper();
  const fs = await BeekeeperProxy.initFS(beekeeperProvider, storage_root);

  const WALLET_OPTIONS = [
    "--wallet-dir",
    `${storage_root}/.beekeeper`,
    "--enable-logs",
    Boolean(enable_logs).toString(),
  ];
  const beekeeperOptions = new beekeeperProvider.StringList();
  WALLET_OPTIONS.forEach((opt) => void beekeeperOptions.push_back(opt));

  const internal_api = new beekeeperProvider.beekeeper_api(beekeeperOptions);
  internal_api.init();

  return new Proxy(
    internal_api as unknown as BeekeeperApi,
    new BeekeeperProxy(fs)
  );
};

export default createBeekeeper;
