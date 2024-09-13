import { beekeeper_api, MainModule, StringList } from "../../build/beekeeper_wasm.web";

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

  public createSession(salt: ArrayBuffer | Uint8Array | Uint8ClampedArray | Int8Array | string): string;

  public createSessionWithoutSalt(): string;

  public closeSession(token: ArrayBuffer | Uint8Array | Uint8ClampedArray | Int8Array | string): string;

  public hasMatchingPrivateKey(token: ArrayBuffer | Uint8Array | Uint8ClampedArray | Int8Array | string, walletName: ArrayBuffer | Uint8Array | Uint8ClampedArray | Int8Array | string, publicKey: ArrayBuffer | Uint8Array | Uint8ClampedArray | Int8Array | string): string;

  public hasWallet(token: ArrayBuffer | Uint8Array | Uint8ClampedArray | Int8Array | string, walletName: ArrayBuffer | Uint8Array | Uint8ClampedArray | Int8Array | string): string;

  public create(sessionToken: ArrayBuffer | Uint8Array | Uint8ClampedArray | Int8Array | string, walletName: ArrayBuffer | Uint8Array | Uint8ClampedArray | Int8Array | string): string;

  public create_with_password(sessionToken: ArrayBuffer | Uint8Array | Uint8ClampedArray | Int8Array | string, walletName: ArrayBuffer | Uint8Array | Uint8ClampedArray | Int8Array | string, explicitPassword: ArrayBuffer | Uint8Array | Uint8ClampedArray | Int8Array | string): string;

  public create_temporary_wallet_with_password(sessionToken: ArrayBuffer | Uint8Array | Uint8ClampedArray | Int8Array | string, walletName: ArrayBuffer | Uint8Array | Uint8ClampedArray | Int8Array | string, explicitPassword: ArrayBuffer | Uint8Array | Uint8ClampedArray | Int8Array | string, isTemporary: boolean): string;

  public importKey(sessionToken: ArrayBuffer | Uint8Array | Uint8ClampedArray | Int8Array | string, walletName: ArrayBuffer | Uint8Array | Uint8ClampedArray | Int8Array | string, key: ArrayBuffer | Uint8Array | Uint8ClampedArray | Int8Array | string): string;

  public importKeys(sessionToken: ArrayBuffer | Uint8Array | Uint8ClampedArray | Int8Array | string, walletName: ArrayBuffer | Uint8Array | Uint8ClampedArray | Int8Array | string, keys: StringList): string;

  public encryptData(sessionToken: ArrayBuffer | Uint8Array | Uint8ClampedArray | Int8Array | string, fromPublicKey: ArrayBuffer | Uint8Array | Uint8ClampedArray | Int8Array | string, toPublicKey: ArrayBuffer | Uint8Array | Uint8ClampedArray | Int8Array | string, walletName: ArrayBuffer | Uint8Array | Uint8ClampedArray | Int8Array | string, content: ArrayBuffer | Uint8Array | Uint8ClampedArray | Int8Array | string): string;

  public decryptData(sessionToken: ArrayBuffer | Uint8Array | Uint8ClampedArray | Int8Array | string, fromPublicKey: ArrayBuffer | Uint8Array | Uint8ClampedArray | Int8Array | string, toPublicKey: ArrayBuffer | Uint8Array | Uint8ClampedArray | Int8Array | string, walletName: ArrayBuffer | Uint8Array | Uint8ClampedArray | Int8Array | string, encryptedContent: ArrayBuffer | Uint8Array | Uint8ClampedArray | Int8Array | string): string;

  public removeKey(sessionToken: ArrayBuffer | Uint8Array | Uint8ClampedArray | Int8Array | string, walletName: ArrayBuffer | Uint8Array | Uint8ClampedArray | Int8Array | string, key: ArrayBuffer | Uint8Array | Uint8ClampedArray | Int8Array | string): string;

  public signDigest(sessionToken: ArrayBuffer | Uint8Array | Uint8ClampedArray | Int8Array | string, sigDigest: ArrayBuffer | Uint8Array | Uint8ClampedArray | Int8Array | string, publicKey: ArrayBuffer | Uint8Array | Uint8ClampedArray | Int8Array | string): string;

  public listWallets(sessionToken: ArrayBuffer | Uint8Array | Uint8ClampedArray | Int8Array | string): string;

  public getPublicKeys(sessionToken: ArrayBuffer | Uint8Array | Uint8ClampedArray | Int8Array | string): string;

  public getInfo(sessionToken: ArrayBuffer | Uint8Array | Uint8ClampedArray | Int8Array | string): string;

  public open(sessionToken: ArrayBuffer | Uint8Array | Uint8ClampedArray | Int8Array | string, walletName: ArrayBuffer | Uint8Array | Uint8ClampedArray | Int8Array | string): string;

  public close(sessionToken: ArrayBuffer | Uint8Array | Uint8ClampedArray | Int8Array | string, walletName: ArrayBuffer | Uint8Array | Uint8ClampedArray | Int8Array | string): string;

  public unlock(sessionToken: ArrayBuffer | Uint8Array | Uint8ClampedArray | Int8Array | string, walletName: ArrayBuffer | Uint8Array | Uint8ClampedArray | Int8Array | string, explicitPassword: ArrayBuffer | Uint8Array | Uint8ClampedArray | Int8Array | string): string;

  public lock(sessionToken: ArrayBuffer | Uint8Array | Uint8ClampedArray | Int8Array | string, walletName: ArrayBuffer | Uint8Array | Uint8ClampedArray | Int8Array | string): string;

  public lockAll(sessionToken: ArrayBuffer | Uint8Array | Uint8ClampedArray | Int8Array | string): string;

  public deleteInstance(): void;

  public setTimeout(sessionToken: ArrayBuffer | Uint8Array | Uint8ClampedArray | Int8Array | string, seconds: number): string;
}
