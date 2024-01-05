import type { BeekeeperModule } from "../beekeeper.js";

export class BeekeeperFileSystem {
  public constructor(
    private readonly fs: BeekeeperModule['FS']
  ) {}

  public sync(): Promise<void> {
    return new Promise((resolve, reject) => {
      this.fs.syncfs((err?: unknown) => {
        if (err) reject(err);

        resolve(undefined);
      });
    });
  }

  public init(walletDir: string): Promise<void> {
    if(!this.fs.analyzePath(walletDir).exists)
      this.fs.mkdir(walletDir);

    if(process.env.ROLLUP_TARGET_ENV === "web")
      this.fs.mount(this.fs.filesystems.IDBFS, {}, walletDir);

    return new Promise((resolve, reject) => {
      this.fs.syncfs(true, (err?: unknown) => {
        if (err) reject(err);

        resolve(undefined);
      });
    });
  }
}
