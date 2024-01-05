export { MainModule, StringList, beekeeper_api } from '../build/beekeeper_wasm.web.js';
import beekeeper from '../build/beekeeper_wasm.web.js';
import type { beekeepermodule } from './beekeeper.js';

export * from './beekeeper.js';

export default beekeeper as beekeepermodule;
