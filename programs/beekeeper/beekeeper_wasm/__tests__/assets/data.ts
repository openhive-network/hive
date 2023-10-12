import type WaxModule from '../../beekeeper';
import type { MainModule } from '../../build/beekeeper';

import type { BeekeeperInstanceHelper, ExtractError } from './run_node_helper.js';

declare global {
  var beekeeper: typeof WaxModule;
  var provider: MainModule;
  var BeekeeperInstanceHelper: BeekeeperInstanceHelper;
  var ExtractError: ExtractError;

  var assert: {
    equal(lhs: any, rhs: any, msg?: string): void;
    ok(val: boolean, msg?: string): void;
    throws(fn: ()=>void, error: Error): void;
  };
}

export const STORAGE_ROOT = '/storage_root';
export const WALLET_OPTIONS = ['--wallet-dir', `${STORAGE_ROOT}/.beekeeper`];
