import type WaxModule from '../../beekeeper';
import type { MainModule } from '../../build/beekeeper';

declare global {
  var beekeeper: typeof WaxModule;
  var provider: MainModule;
}

export const STORAGE_ROOT = '/storage_root';
export const WALLET_OPTIONS = ['--wallet-dir', `${STORAGE_ROOT}/.beekeeper`];
