export * from '../../../build_wasm/beekeeper';

import { MainModule } from '../../../build_wasm/beekeeper';

export interface FileSystemType {
  MEMFS: {};
  NODEFS: {};
  IDBFS: {};
}

interface BeekeeperModule extends MainModule {
  FS: {
    // Required types from https://github.com/DefinitelyTyped/DefinitelyTyped/blob/master/types/emscripten/index.d.ts for IDBFS usage
    mkdir(path: string, mode?: number): any;
    chdir(path: string): void;
    mount(type: FileSystemType, opts: any, mountpoint: string): any;
    syncfs(populate: boolean, callback: (e: any) => any): void;
    syncfs(callback: (e: any) => any, populate?: boolean): void;
    filesystems: FileSystemType;
  }
}

declare function beekeepermodule(): Promise<BeekeeperModule>;

export default beekeepermodule;
