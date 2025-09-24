import { beekeeper_api, MainModule, StringList } from "../../dist/build/beekeeper_wasm.common.js";

export declare class ExtractError extends Error {
  constructor(parsed: object);

  public parsed: object;
}

export interface IBeekeeperInstanceHelperConstructorSimplified {
  new(options: string[]): BeekeeperInstanceHelper;
}

export declare class BeekeeperInstanceHelper {
  public get implicitSessionToken(): string;

  public get instance(): beekeeper_api;

  public get version(): string;

  public set setAcceptError(acceptError: boolean);

  public static for(provider: MainModule): IBeekeeperInstanceHelperConstructorSimplified;

  public constructor(provider: MainModule, options: string[]);

  public createSession(salt: string): string;

  public createSessionWithoutSalt(): string;

  public closeSession(token: string): string;

  public hasMatchingPrivateKey(token: string, walletName: string, publicKey: string): boolean;

  public hasWallet(token: string, walletName: string): boolean;

  public create(sessionToken: string, walletName: string): string;

  public create_with_password(sessionToken: string, walletName: string, explicitPassword: string): string;

  public create_temporary_wallet_with_password(sessionToken: string, walletName: string, explicitPassword: string, isTemporary: boolean): string;

  public importKey(sessionToken: string, walletName: string, key: string): string;

  public importKeys(sessionToken: string, walletName: string, keys: StringList): string;

  public encryptData(sessionToken: string, fromPublicKey: string, toPublicKey: string, walletName: string, content: string): string;

  public decryptData(sessionToken: string, fromPublicKey: string, toPublicKey: string, walletName: string, encryptedContent: string): string;

  public removeKey(sessionToken: string, walletName: string, key: string): string;

  public signDigest(sessionToken: string, sigDigest: string, publicKey: string): string;

  public listWallets(sessionToken: string): { wallets: Array<{ name: string, unlocked: boolean }> };

  public getPublicKeys(sessionToken: string): { keys: Array<{ public_key: string }> };

  public getInfo(sessionToken: string): { now: string, timeout_time: string };

  public open(sessionToken: string, walletName: string): string;

  public close(sessionToken: string, walletName: string): string;

  public unlock(sessionToken: string, walletName: string, explicitPassword: string): string;

  public lock(sessionToken: string, walletName: string): string;

  public lockAll(sessionToken: string): string;

  public deleteInstance(): void;

  public setTimeout(sessionToken: string, seconds: number): string;

  public isWalletUnlocked(sessionToken: string, walletName: string): { unlocked: boolean};
}
