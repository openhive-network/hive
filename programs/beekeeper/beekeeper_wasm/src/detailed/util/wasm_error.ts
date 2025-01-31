import { BeekeeperError } from "../errors.js";

// TODO: Check for errno on Windows
const stringifyError = (error: unknown): string => {
  if (error instanceof Error)
    return error.message;

  const name = (error && typeof error === "object" && "name" in error) ? error.name : undefined;

  if (name === "ErrnoError" && "errno" in (error as object)) {
    const errno = (error as any).errno as number;
    return `ErrnoError: #${errno}`;
  }

  return "Unknown error" + (name ? `: ${name}` : "");
};

export const preserveErrorStack = <T extends Error>(caughtError: unknown, newError: T): T => {
  if (typeof caughtError !== "object" || caughtError === null || !("stack" in caughtError))
    return newError;

  return Object.assign(newError, { stack: caughtError.stack });
};

export const safeWasmCall = <T extends () => any>(fn: T, errorCommentDuring = "Wasm call"): ReturnType<T> => {
  try {
    return fn()
  } catch (error) {
    throw preserveErrorStack(error, new BeekeeperError(`Error during ${errorCommentDuring}: ${stringifyError(error)}`));
  }
};

export const safeAsyncWasmCall = async <T extends () => any>(fn: T, errorCommentDuring = "Wasm call"): Promise<ReturnType<T>> => {
  try {
    return await fn();
  } catch (error) {
    throw preserveErrorStack(error, new BeekeeperError(`Error during ${errorCommentDuring}: ${stringifyError(error)}`));
  }
};
