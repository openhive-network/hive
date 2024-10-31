import type { BeekeeperModule } from "../beekeeper.js";
import { safeWasmCall } from "../util/wasm_error.js";

export class BeekeeperFileSystem {
  public constructor(
    private readonly fs: BeekeeperModule['FS']
  ) {}

  public sync(): Promise<void> {
    return new Promise((resolve, reject) => {
      safeWasmCall(() => this.fs.syncfs((err?: unknown) => {
        if (err) reject(err);

        resolve(undefined);
      }));
    });
  }

  public init(walletDir: string): Promise<void> {
    if(!safeWasmCall(() => this.fs.analyzePath(walletDir).exists))
      safeWasmCall(() => this.fs.mkdir(walletDir));

    if(process.env.ROLLUP_TARGET_ENV === "web")
      safeWasmCall(() => this.fs.mount(this.fs.filesystems.IDBFS, {}, walletDir));

    return new Promise((resolve, reject) => {
      safeWasmCall(() => this.fs.syncfs(true, (err?: unknown) => {
        if (err) reject(err);

        resolve(undefined);
      }));
    });
  }
}
