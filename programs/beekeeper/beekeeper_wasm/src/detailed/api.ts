import type { MainModule, beekeeper_api } from "../build/beekeeper.common";

import { BeekeeperError } from "./errors.js";
import { BeekeeperFileSystem } from "./fs.js";
import { IBeekeeperInstance, IBeekeeperOptions, IBeekeeperSession } from "./interfaces.js";
import { BeekeeperSession } from "./session.js";
import { safeWasmCall } from './util/wasm_error.js';

// We would like to expose our api using BeekeeperInstance interface, but we would not like to expose users a way of creating instance of BeekeeperApi
export class BeekeeperApi implements IBeekeeperInstance {
  public readonly fs: BeekeeperFileSystem;
  public api!: Readonly<beekeeper_api>;

  public readonly sessions: Map<string, BeekeeperSession> = new Map();

  public constructor(
    private readonly provider: MainModule,
    isWebEnvironment: boolean
  ) {
    this.fs = new BeekeeperFileSystem(this.provider.FS, isWebEnvironment);
  }

  public get version(): string {
    return process.env.npm_package_version as string;
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
        throw new BeekeeperError(`${error instanceof Error ? error.name : 'Unknown error'}: Could not extract the result from the beekeeper response: "${json}"`);

      throw error;
    }
  }

  public async init({ storageRoot, enableLogs, unlockTimeout }: IBeekeeperOptions) {
    await this.fs.init(storageRoot);

    const WALLET_OPTIONS = ['--wallet-dir', `${storageRoot}/.beekeeper`, '--enable-logs', Boolean(enableLogs).toString(), '--unlock-timeout', String(unlockTimeout)];

    const beekeeperOptions = new this.provider.StringList();
    WALLET_OPTIONS.forEach((opt) => void beekeeperOptions.push_back(opt));

    this.api = new this.provider.beekeeper_api(beekeeperOptions);
    safeWasmCall(() => beekeeperOptions.delete(), "StringList WASM object deletion");

    this.extract(safeWasmCall(() => this.api.init() as string, "WASM api initialization"));
  }

  public createSession(salt: string): IBeekeeperSession {
    const { token } = this.extract(safeWasmCall(() => this.api.create_session(salt) as string, "session creation")) as { token: string };
    const session = new BeekeeperSession(this, token);

    this.sessions.set(token, session);

    return session;
  }

  public closeSession(token: string): void {
    if(!this.sessions.delete(token))
      throw new BeekeeperError(`This Beekeeper API instance is not the owner of session identified by token: "${token}"`);

    this.extract(safeWasmCall(() => this.api.close_session(token) as string, "session closing"));
  }

  public async delete(): Promise<void> {
    for(const session of this.sessions.values())
      session.close();

    safeWasmCall(() => this.api.delete(), "WASM api deletion");

    await this.fs.sync();
  }
}
