import { BeekeeperError } from "../errors.js";

export const safeWasmCall = <T extends () => any>(fn: T): ReturnType<T> => {
  try {
    return fn()
  } catch (error) {
    throw new BeekeeperError(`Error during Wasm call: ${error instanceof Error ? error.message : error}`);
  }
};

export const safeAsyncWasmCall = async <T extends () => any>(fn: T): Promise<ReturnType<T>> => {
  try {
    return await fn();
  } catch (error) {
    throw new BeekeeperError(`Error during Wasm call: ${error instanceof Error ? error.message : error}`);
  }
};
