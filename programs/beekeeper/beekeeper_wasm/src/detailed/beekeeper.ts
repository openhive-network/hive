import type Beekeeper from "../build/beekeeper_wasm.common";

import { BeekeeperApi } from "./api.js";
import { IBeekeeperInstance, IBeekeeperOptions } from "./interfaces.js";
import { safeAsyncWasmCall } from "./util/wasm_error.js";

const DEFAULT_BEEKEEPER_OPTIONS: Omit<IBeekeeperOptions, 'storageRoot' | 'wasmLocation'> = {
  enableLogs: false,
  unlockTimeout: 900,
  inMemory: false
};

interface IOptionalModuleArgs {
  wasmBinary?: Uint8Array;
  locateFile?: (path: string, scriptDirectory: string) => string;
}

const createBeekeeper = async(
  beekeeperContstructor: typeof Beekeeper,
  storageRoot: string,
  ModuleExt: IOptionalModuleArgs = {},
  isWebEnvironment: boolean,
  options: Partial<IBeekeeperOptions> = {}
): Promise<IBeekeeperInstance> => {
  const beekeeperProvider = await safeAsyncWasmCall(() => beekeeperContstructor(ModuleExt), "Beekeeper WASM module loading");
  const api = new BeekeeperApi(beekeeperProvider, { ...DEFAULT_BEEKEEPER_OPTIONS, storageRoot, ...options }, isWebEnvironment);

  await api.init();

  return api;
};

export default createBeekeeper;
