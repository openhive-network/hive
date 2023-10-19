import Beekeeper from "../beekeeper.js";
import { BeekeeperApi } from "./api.js";
import { IBeekeeperInstance, IBeekeeperOptions } from "../interfaces.js";

export const DEFAULT_BEEKEEPER_OPTIONS: IBeekeeperOptions = {
  storageRoot: "/storage_root",
  enableLogs: true,
  unlockTimeout: 900
};

const createBeekeeper = async(
  options: Partial<IBeekeeperOptions> = {}
): Promise<IBeekeeperInstance> => {
  const beekeeperProvider = await Beekeeper();
  const api = new BeekeeperApi(beekeeperProvider);

  await api.init({ ...DEFAULT_BEEKEEPER_OPTIONS, ...options });

  return api;
};

export default createBeekeeper;
