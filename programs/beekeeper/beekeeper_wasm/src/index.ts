// Input file for both: Web and Node.js

// @ts-expect-error ts(6133) Types used in JSDoc generation
import createBeekeeperBase, { type BeekeeperError, type IBeekeeperOptions, type IBeekeeperInstance } from "./detailed/index.js";

// Note: This import will be replaced with Web/Node.js version upon bundling
import Beekeeper from "./build/beekeeper_wasm.common.js";

// This variable will be replaced during bundling based on the environment
export const DEFAULT_STORAGE_ROOT: string = process.env.DEFAULT_STORAGE_ROOT as string;

export * from "./detailed/index.js";

// Polyfill for web workers in WASM
declare global {
  var WorkerGlobalScope: /* object extends */ EventTarget | undefined;
}
const ENVIRONMENT_IS_WORKER = typeof WorkerGlobalScope != 'undefined';

const getModuleExt = () => {
  // Warning: important change is moving conditional ternary expression outside of URL constructor call, what confused parcel analyzer.
  // Seems it must have simple variables & literals present to correctly translate code.
  const wasmFilePath = (ENVIRONMENT_IS_WORKER ? new URL("./build/beekeeper_wasm.common.wasm", self.location.href) : new URL("./build/beekeeper_wasm.common.wasm", import.meta.url)).href;
  // Fallback for client-bundled inlined WASM, e.g. when using webpack
  let wasmBinary: Buffer | undefined;
  if (wasmFilePath.startsWith("data:application/wasm;base64,"))
      wasmBinary = Buffer.from(wasmFilePath.slice(29), "base64");

  return {
    locateFile(path: string, scriptDirectory: string): string {
      if (path === "beekeeper_wasm.common.wasm") {
        return wasmFilePath;
      }
      return scriptDirectory + path;
    },
    wasmBinary
  };
};

/**
 * Creates a Beekeeper instance able to own sessions and wallets
 *
 * @param {?Partial<IBeekeeperOptions>} options options passed to the WASM Beekeeper
 *
 * @returns {Promise<IBeekeeperInstance>} Beekeeper API Instance
 *
 * @throws {BeekeeperError} on any beekeeper API-related error (error parsing response, invalid input, timeout error, fs sync error etc.)
 */
const createBeekeeper = async(options?: Partial<IBeekeeperOptions>): Promise<IBeekeeperInstance> => {
  return createBeekeeperBase(Beekeeper, DEFAULT_STORAGE_ROOT, getModuleExt(), process.env.ROLLUP_TARGET_ENV === "web", options);
};

export default createBeekeeper;
