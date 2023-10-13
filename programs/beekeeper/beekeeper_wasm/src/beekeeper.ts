export { MainModule, StringList, beekeeper_api } from '../build/beekeeper_wasm.js';
import type { MainModule } from '../build/beekeeper_wasm.js';

import beekeeper from '../build/beekeeper_wasm.js';

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
    chdir(path: string): void;
    mount(type: TFileSystemType, opts: any, mountpoint: string): any;
    syncfs(populate: boolean, callback: (e: any) => any): void;
    syncfs(callback: (e: any) => any, populate?: boolean): void;
    filesystems: FileSystemType;
  }
}

export type beekeepermodule = () => Promise<BeekeeperModule>;

export default beekeeper as beekeepermodule;
