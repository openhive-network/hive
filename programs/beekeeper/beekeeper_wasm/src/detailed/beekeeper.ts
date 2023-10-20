import Beekeeper from "../beekeeper.js";
import { BeekeeperApi } from "./api.js";
import { IBeekeeperInstance, IBeekeeperOptions } from "../interfaces.js";

export const DEFAULT_BEEKEEPER_OPTIONS: IBeekeeperOptions = {
  storageRoot: "/storage_root",
  enableLogs: true,
  unlockTimeout: 900
};

/**
 * Creates a Beekeeper instance able to own sessions and wallets
 *
 * @param {?Partial<IBeekeeperOptions>} options options passed to the WASM Beekeeper
 *
 * @returns {Promise<IBeekeeperInstance>} Beekeeper API Instance
 *
 * @throws {import("../errors").BeekeeperError} on any beekeeper API-related error (error parsing response, invalid input, timeout error, fs sync error etc.)
 */
const createBeekeeper = async(
  options: Partial<IBeekeeperOptions> = {}
): Promise<IBeekeeperInstance> => {
  const beekeeperProvider = await Beekeeper();
  const api = new BeekeeperApi(beekeeperProvider);

  await api.init({ ...DEFAULT_BEEKEEPER_OPTIONS, ...options });

  return api;
};

export default createBeekeeper;
