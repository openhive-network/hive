// We only want to import types here!
import type BeekeeperModule from '../../build/beekeeper_wasm.web';
import { MainModule } from '../../build/beekeeper_wasm.web';
import { IBeekeeperInstance } from '../../dist/node';

type TMainModule = () => Promise<MainModule>;
export type TEnvType = 'web' | 'node';

export interface IBeekeeperGlobals {
  beekeeper: typeof BeekeeperModule;
  factory: IBeekeeperInstance;
};

export interface IBeekeeperWasmGlobals {
  provider: MainModule;
};

declare global {
  function createBeekeeperTestFor (env: TEnvType): Promise<IBeekeeperGlobals>;
  function createBeekeeperWasmTestFor (env: TEnvType): Promise<IBeekeeperWasmGlobals>;
};

globalThis.createBeekeeperTestFor = async function createBeekeeperTestFor (env: TEnvType): Promise<IBeekeeperGlobals> {
  const locBeekeeper = env === 'web' ? '../../dist/web' : '../../dist/node';

  const beekeeper = await import(locBeekeeper) as typeof import('../../dist/node');

  const bk = await (env === 'web' ? beekeeper : beekeeper.default({ storageRoot: '.beekeeper' })) as IBeekeeperInstance;

  return {
    beekeeper: beekeeper,
    factory: bk
  };
};

globalThis.createBeekeeperWasmTestFor = async function createBeekeeperWasmTestFor (_env: TEnvType): Promise<IBeekeeperWasmGlobals> {
  const wasm = await import('../../build/beekeeper_wasm.web') as unknown as MainModule;

  const provider = await (wasm as unknown as { default: TMainModule }).default();

  return {
    provider
  };
};

export {};
