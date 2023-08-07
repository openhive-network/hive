export * from '../../../build_wasm/beekeeper';

import { MainModule } from '../../../build_wasm/beekeeper';

declare function waxmodule(): Promise<MainModule>;

export default waxmodule;
