export * from "./detailed/index.js";

// @ts-expect-error ts(6133) Types used in JSDoc generation
import createBeekeeperBase, { type BeekeeperError, type IBeekeeperOptions, type IBeekeeperInstance } from "./detailed/index.js";
import Beekeeper from "./beekeeper_module.js";

// This will be empty when SSR is disabled (client-side), but enable static import for SSR
import possibleFs from 'node:fs/promises';

const isSSR = typeof (import.meta as any).env === "object" && (import.meta as any).env.SSR;

import resolvedUrl from 'beekeeper.common.wasm?url';

const moduleArgs = (async () => {
  let wasmBinary: Buffer | undefined;

  if (isSSR)
    wasmBinary = await possibleFs.readFile('beekeeper.common.wasm');

  return {
    wasmBinary,
    locateFile: (path: string, scriptDirectory: string): string => {
      if (path === "beekeeper.common.wasm")
        return resolvedUrl as unknown as string;

      return scriptDirectory + path;
    }
  };
})();

export const DEFAULT_STORAGE_ROOT: string = process.env.DEFAULT_STORAGE_ROOT as string;

/**
 * Creates a Beekeeper instance able to own sessions and wallets
 *
 * @param {?Partial<IBeekeeperOptions>} options options passed to the WASM Beekeeper
 *
 * @returns {Promise<IBeekeeperInstance>} Beekeeper API Instance
 *
 * @throws {BeekeeperError} on any beekeeper API-related error (error parsing response, invalid input, timeout error, fs sync error etc.)
 */
export const createBeekeeper = async(options?: Partial<IBeekeeperOptions>): Promise<IBeekeeperInstance> => {
  return createBeekeeperBase(Beekeeper, DEFAULT_STORAGE_ROOT, Object.assign({}, await moduleArgs), process.env.ROLLUP_TARGET_ENV === "web", options);
};

