// Note: This import will be replaced with Node.js version upon bundling
import beekeeper from 'beekeeper_wasm/beekeeper_wasm.web.js';

import type { MainModule } from 'beekeeper_wasm/beekeeper_wasm.web.js';

export interface FileSystemType {
  MEMFS: {};
  NODEFS: {};
  IDBFS: {};
}

export type TFileSystemType = FileSystemType['MEMFS'] | FileSystemType['NODEFS'] | FileSystemType['IDBFS'];

export interface IAnalyzePathNodeObject {
  id: number;
  mode: number;
  node_ops: object;
  path: string | null;
}

export interface IAnalyzePathData {
  isRoot: boolean;
  exists: boolean;
  error: 0 | number;
  name: string;
  path: null | string;
  object: null | IAnalyzePathNodeObject;
  parentExists: boolean;
  parentPath: null | string;
  parentObject: null | IAnalyzePathNodeObject;
}

export interface BeekeeperModule extends MainModule {
  FS: {
    // Required types from https://github.com/DefinitelyTyped/DefinitelyTyped/blob/master/types/emscripten/index.d.ts for IDBFS usage
    analyzePath(path: string): IAnalyzePathData;
    mkdir(path: string, mode?: number): any;
    readdir(path: string): string;
    readFile(path: string, opts: { encoding: "binary"; flags?: string | undefined }): Uint8Array;
    readFile(path: string, opts: { encoding: "utf8"; flags?: string | undefined }): string;
    readFile(path: string, opts?: { flags?: string | undefined }): Uint8Array;
    stat(path: string, dontFollow?: boolean): any;
    writeFile(path: string, data: string | ArrayBufferView, opts?: { flags?: string | undefined }): void;
    chdir(path: string): void;
    mount(type: TFileSystemType, opts: any, mountpoint: string): any;
    syncfs(populate: boolean, callback: (e: any) => any): void;
    syncfs(callback: (e: any) => any, populate?: boolean): void;
    filesystems: FileSystemType;
  }
}

// Note: This import will be replaced with Node.js version upon bundling
export { MainModule, StringList, beekeeper_api } from 'beekeeper_wasm/beekeeper_wasm.web.js';

export type beekeepermodule = () => Promise<BeekeeperModule>;

export default beekeeper as beekeepermodule;
