import type Beekeeper from "../beekeeper.js";

import { BeekeeperApi } from "./api.js";
import { IBeekeeperInstance, IBeekeeperOptions } from "../interfaces.js";
import { safeAsyncWasmCall } from "../util/wasm_error.js";

const DEFAULT_BEEKEEPER_OPTIONS: Omit<IBeekeeperOptions, 'storageRoot'> = {
  enableLogs: true,
  unlockTimeout: 900
};

const createBeekeeper = async(
  beekeeperContstructor: typeof Beekeeper,
  storageRoot: string,
  options: Partial<IBeekeeperOptions> = {}
): Promise<IBeekeeperInstance> => {
  const beekeeperProvider = await safeAsyncWasmCall(() => beekeeperContstructor());
  const api = new BeekeeperApi(beekeeperProvider);

  await api.init({ ...DEFAULT_BEEKEEPER_OPTIONS, storageRoot, ...options });

  return api;
};

export default createBeekeeper;
