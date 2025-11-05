/* @terser-ignore */
// We want to skip minification for this file to preserve string concatination hacks used for WASM loading specifically in Vite environment

// @ts-expect-error ts(6133) Types used in JSDoc generation
import createBeekeeperBase, { type BeekeeperError, type IBeekeeperOptions, type IBeekeeperInstance } from "./detailed/index.js";

// Note: This import will be replaced with Web/Node.js version upon bundling
import Beekeeper from "./build/beekeeper_wasm.common.js";

// This variable will be replaced during bundling based on the environment
export const DEFAULT_STORAGE_ROOT: string = process.env.DEFAULT_STORAGE_ROOT as string;

export * from "./detailed/index.js";

const getModuleExt = async(fileLocation?: string) => {
  if ((import.meta as any).client || (!(import.meta as any).client && typeof (import.meta as any).env === "object" && !(import.meta as any).env?.SSR)) {
    // This is import style specific to Vite. Simple hack with concatination to avoid TypeScript & Rollup bundler related errors
    const filePath = fileLocation ?? (await import('./build/beekeeper_wasm.common.wasm' + '?url')).default;
    return {
      locateFile(path: string, scriptDirectory: string): string {
        if (path === "beekeeper_wasm.common.wasm") {
          return filePath;
        }
        return scriptDirectory + path;
      }
    };
  }

  // Load WASM without any rewriting filepaths in non-browser environments
  if (typeof window === "undefined" && typeof process === "object")
    // We explicitly define the default locateFile function here to override wrong emscripten behavior of falling back
    // to new URL(..., import.meta.url) which fails in webpack
    return {
      locateFile: (path: string, scriptDirectory: string): string => scriptDirectory + path
    };

  // Warning: important change is moving conditional ternary expression outside of URL constructor call, what confused parcel analyzer.
  // Seems it must have simple variables & literals present to correctly translate code.
  const wasmFilePath = fileLocation ?? new URL("./build/beekeeper_wasm.common.wasm", import.meta.url).href;
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
  const { wasmLocation, ...otherOptions } = options || {};

  return createBeekeeperBase(Beekeeper, DEFAULT_STORAGE_ROOT, await getModuleExt(wasmLocation), process.env.ROLLUP_TARGET_ENV === "web", otherOptions);
};

export default createBeekeeper;
