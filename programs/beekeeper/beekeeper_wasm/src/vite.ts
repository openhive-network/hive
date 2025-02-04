export * from "./detailed/index.js";

// @ts-expect-error ts(6133) Types used in JSDoc generation
import createBeekeeperBase, { type BeekeeperError, type IBeekeeperOptions, type IBeekeeperInstance } from "./detailed/index.js";
import Beekeeper from "./build/beekeeper.common.js";

const moduleArgs = (async () => {
  // Resolve WASM url only for Nuxt client (first condition) or Vite client (second condition)
  if ((import.meta as any).client || (!("client" in import.meta) && typeof (import.meta as any).env === "object" && "SSR" in (import.meta as any).env)) {
      const resolvedUrl = (await import('./build/beekeeper.common.wasm' + '?url')).default;

      return {
          locateFile: (path, scriptDirectory) => {
              if (path === "beekeeper.common.wasm")
                  return resolvedUrl;
              return scriptDirectory + path;
          }
      };
  } else {
      return {};
  }
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
  // Remember to keep Object.assign to always create new ModuleArg object passed to the WASM Emscripten wrapper on which new objects will be initialized and stored
  // Otherwise you will get strange errors, such as: "Cannot register type XXX twice"
  return createBeekeeperBase(Beekeeper, DEFAULT_STORAGE_ROOT, Object.assign({}, await moduleArgs), process.env.ROLLUP_TARGET_ENV === "web", options);
};

