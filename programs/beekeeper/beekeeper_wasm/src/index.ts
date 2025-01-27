// Input file for both: Web and Node.js

// @ts-expect-error ts(6133) Types used in JSDoc generation
import createBeekeeperBase, { type BeekeeperError, type IBeekeeperOptions, type IBeekeeperInstance } from "./detailed/index.js";
import Beekeeper from "./beekeeper_module.js";

// This variable will be replaced during bundling based on the environment
export const DEFAULT_STORAGE_ROOT: string = process.env.DEFAULT_STORAGE_ROOT as string;

export * from "./detailed/index.js";

/**
 * Creates a Beekeeper instance able to own sessions and wallets
 *
 * @param {?Partial<IBeekeeperOptions>} options options passed to the WASM Beekeeper
 *
 * @returns {Promise<IBeekeeperInstance>} Beekeeper API Instance
 *
 * @throws {BeekeeperError} on any beekeeper API-related error (error parsing response, invalid input, timeout error, fs sync error etc.)
 */
const createBeekeeper = createBeekeeperBase.bind(undefined, Beekeeper, DEFAULT_STORAGE_ROOT, process.env.ROLLUP_TARGET_ENV === "web");

export default createBeekeeper;
