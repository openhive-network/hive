import type { BeekeeperModule } from "../beekeeper.web.js";

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
    this.fs.mkdir(walletDir);

    const fsType = process.env.ROLLUP_TARGET_ENV === "web" ? 'IDBFS' : 'NODEFS';
    this.fs.mount(this.fs.filesystems[fsType], {}, walletDir);

    return new Promise((resolve, reject) => {
      this.fs.syncfs(true, (err?: unknown) => {
        if (err) reject(err);

        resolve(undefined);
      });
    });
  }
}
