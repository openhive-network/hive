import type BeekeeperModule from '../../build/beekeeper_wasm';
import type { MainModule } from '../../build/beekeeper_wasm';
import type BeekeeperFactory from '../../dist/index';
import type BeekeeperFactory2 from '../../dist/detailed/index';

import type { BeekeeperInstanceHelper, ExtractError } from './run_node_helper.js';

declare global {
  var beekeeper: typeof BeekeeperModule;
  var provider: MainModule;
  var factory: typeof BeekeeperFactory;
  var factory2: typeof BeekeeperFactory2;
  var BeekeeperInstanceHelper: BeekeeperInstanceHelper;
  var ExtractError: ExtractError;

  var assert: {
    equal(lhs: any, rhs: any, msg?: string): void;
    ok(val: boolean, msg?: string): void;
    throws(fn: ()=>void, error: Error): void;
  };
}

export const STORAGE_ROOT = '/storage_root';
export const WALLET_OPTIONS = ['--wallet-dir', `${STORAGE_ROOT}/directory with spaces/.beekeeper`];
