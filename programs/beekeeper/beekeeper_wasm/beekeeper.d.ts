export * from '../../../build_wasm/beekeeper';

import { MainModule } from '../../../build_wasm/beekeeper';

declare function beekeepermodule(): Promise<MainModule>;

export default beekeepermodule;
