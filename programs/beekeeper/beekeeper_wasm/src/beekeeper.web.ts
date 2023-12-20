export { MainModule, StringList, beekeeper_api } from '../build/beekeeper_wasm.web.js';
import type { MainModule } from '../build/beekeeper_wasm.web.js';

import beekeeper from '../build/beekeeper_wasm.web.js';

export interface FileSystemType {
  MEMFS: {};
  NODEFS: {};
  IDBFS: {};
}

export type TFileSystemType = FileSystemType['MEMFS'] | FileSystemType['NODEFS'] | FileSystemType['IDBFS'];

export interface BeekeeperModule extends MainModule {
  FS: {
    // Required types from https://github.com/DefinitelyTyped/DefinitelyTyped/blob/master/types/emscripten/index.d.ts for IDBFS usage
    mkdir(path: string, mode?: number): any;
    readdir(path: string): string;
    readFile(path: string, opts: { encoding: "binary"; flags?: string | undefined }): Uint8Array;
    readFile(path: string, opts: { encoding: "utf8"; flags?: string | undefined }): string;
    readFile(path: string, opts?: { flags?: string | undefined }): Uint8Array;
    writeFile(path: string, data: string | ArrayBufferView, opts?: { flags?: string | undefined }): void;
    chdir(path: string): void;
    mount(type: TFileSystemType, opts: any, mountpoint: string): any;
    syncfs(populate: boolean, callback: (e: any) => any): void;
    syncfs(callback: (e: any) => any, populate?: boolean): void;
    filesystems: FileSystemType;
  }
}

export type beekeepermodule = () => Promise<BeekeeperModule>;

export default beekeeper as beekeepermodule;
