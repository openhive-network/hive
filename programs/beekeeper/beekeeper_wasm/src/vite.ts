export * from "./detailed/index.js";

// @ts-expect-error ts(6133) Types used in JSDoc generation
import createBeekeeperBase, { type BeekeeperError, type IBeekeeperOptions, type IBeekeeperInstance } from "./detailed/index.js";
import Beekeeper from "./build/beekeeper_wasm.common.js";

const moduleLocation = (async () => {
  if ((import.meta as any).client || (!("client" in import.meta) && typeof (import.meta as any).env === "object" && "SSR" in (import.meta as any).env))
    return (await import('./build/beekeeper_wasm.common.wasm' + '?url')).default;
})();

const getModuleExt = async(fileLocation?: string) => {
  // Warning: important change is moving conditional ternary expression outside of URL constructor call, what confused parcel analyzer.
  // Seems it must have simple variables & literals present to correctly translate code.
  const wasmFilePath = fileLocation ?? await moduleLocation;
  if (wasmFilePath === undefined)
    return {};

  // Fallback for client-bundled inlined WASM, e.g. when using webpack
  let wasmBinary: Uint8Array | undefined;
  if (wasmFilePath.startsWith("data:application/wasm;base64,")) {
    const base64 = wasmFilePath.slice(29);
    const binaryString = atob(base64);
    const len = binaryString.length;
    const bytes = new Uint8Array(len);
    for (let i = 0; i < len; ++i)
      bytes[i] = binaryString.charCodeAt(i);
    wasmBinary = bytes;
  }

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

export const DEFAULT_STORAGE_ROOT: string = "/storage_root";

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
  const { wasmLocation, ...otherOptions } = options || {};

  // Remember to keep Object.assign to always create new ModuleArg object passed to the WASM Emscripten wrapper on which new objects will be initialized and stored
  // Otherwise you will get strange errors, such as: "Cannot register type XXX twice"
  return createBeekeeperBase(Beekeeper, DEFAULT_STORAGE_ROOT, await getModuleExt(wasmLocation), true, otherOptions);
};
