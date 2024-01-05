import type Beekeeper from "../beekeeper.node.js";

import { BeekeeperApi } from "./api.js";
import { IBeekeeperInstance, IBeekeeperOptions } from "../interfaces.js";

const DEFAULT_BEEKEEPER_OPTIONS: Omit<IBeekeeperOptions, 'storageRoot'> = {
  enableLogs: true,
  unlockTimeout: 900
};

const createBeekeeper = async(
  beekeeperContstructor: typeof Beekeeper,
  storageRoot: string,
  options: Partial<IBeekeeperOptions> = {}
): Promise<IBeekeeperInstance> => {
  const beekeeperProvider = await beekeeperContstructor();
  const api = new BeekeeperApi(beekeeperProvider);

  await api.init({ ...DEFAULT_BEEKEEPER_OPTIONS, storageRoot, ...options });

  return api;
};

export default createBeekeeper;
