import { BeekeeperError } from "../errors.js";

// TODO: Check for errno on Windows
const stringifyError = (error: unknown): string => {
  if (error instanceof Error)
    return error.message;

  const name = error && typeof error === "object" && "name" in error ? error.name : undefined;

  if (name === "ErrnoError" && "errno" in (error as object)) {
    const errno = (error as any).errno as number;
    return `ErrnoError: #${errno}`;
  }

  return "Unknown error" + (name ? `: ${name}` : "");
};

export const safeWasmCall = <T extends () => any>(fn: T): ReturnType<T> => {
  try {
    return fn()
  } catch (error) {
    throw new BeekeeperError(`Error during Wasm call: ${stringifyError(error)}`);
  }
};

export const safeAsyncWasmCall = async <T extends () => any>(fn: T): Promise<ReturnType<T>> => {
  try {
    return await fn();
  } catch (error) {
    throw new BeekeeperError(`Error during Wasm call: ${stringifyError(error)}`);
  }
};
