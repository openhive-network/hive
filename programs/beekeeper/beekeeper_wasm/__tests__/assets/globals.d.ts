// We only want to import types here!!
import { MainModule } from '../../build/beekeeper_wasm.web';
import { IBeekeeperInstance } from '../../dist/bundle/web';
import * as BeekeeperModule from '../../dist/bundle/web';
import { BeekeeperInstanceHelper, ExtractError } from './run_node_helper.js';

export type TEnvType = 'web' | 'node';

export interface IBeekeeperGlobals {
  provider: typeof BeekeeperModule;
  beekeeper: IBeekeeperInstance;
}

export interface IBeekeeperWasmGlobals {
  provider: MainModule;
  ExtractError: typeof ExtractError;
  BeekeeperInstanceHelper: typeof BeekeeperInstanceHelper;
}

declare global {
  function createBeekeeperTestFor (env: TEnvType): Promise<IBeekeeperGlobals>;
  function createBeekeeperWasmTestFor (env: TEnvType): Promise<IBeekeeperWasmGlobals>;
}
