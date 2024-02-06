// @ts-expect-error ts(6133) Type BeekeeperError is used in JSDoc
import type { BeekeeperError } from "./errors";

import createBeekeeperBase from "./detailed/beekeeper.js";
import Beekeeper from "./beekeeper.js";

export * from "./interfaces.js";
export * from "./errors.js";

export const DEFAULT_STORAGE_ROOT = "/storage_root";

/**
 * Creates a Beekeeper instance able to own sessions and wallets
 *
 * @param {?Partial<IBeekeeperOptions>} options options passed to the WASM Beekeeper
 *
 * @returns {Promise<IBeekeeperInstance>} Beekeeper API Instance
 *
 * @throws {BeekeeperError} on any beekeeper API-related error (error parsing response, invalid input, timeout error, fs sync error etc.)
 */
const createBeekeeper = createBeekeeperBase.bind(undefined, Beekeeper, DEFAULT_STORAGE_ROOT);

export default createBeekeeper;
