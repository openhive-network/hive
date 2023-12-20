import type BeekeeperModule from '../../build/beekeeper_wasm.web';
import type { MainModule } from '../../build/beekeeper_wasm.web';
import type BeekeeperFactory from '../../dist/web';

declare global {
  var beekeeper: typeof BeekeeperModule;
  var provider: MainModule;
  var factory: typeof BeekeeperFactory;
  var BeekeeperInstanceHelper: object;
  var ExtractError: Error;

  var assert: {
    equal(lhs: any, rhs: any, msg?: string): void;
    ok(val: boolean, msg?: string): void;
    throws(fn: ()=>void, error: Error): void;
  };
}

export const STORAGE_ROOT = '/storage_root';
export const WALLET_OPTIONS = ['--wallet-dir', `${STORAGE_ROOT}/directory with spaces/.beekeeper`];
