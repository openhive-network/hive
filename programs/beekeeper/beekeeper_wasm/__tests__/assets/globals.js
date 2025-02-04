import { BeekeeperInstanceHelper, ExtractError } from './run_node_helper.js';

globalThis.createBeekeeperTestFor = async function createBeekeeperTestFor (env) {
  const locBeekeeper = env === 'web' ? '../../dist/bundle/web' : '../../dist/bundle/node';

  const beekeeper = await import(locBeekeeper);

  const bk = await beekeeper.default({ storageRoot: env === "web" ? "/storage_root" : '.beekeeper', enableLogs: false });

  return {
    provider: beekeeper,
    beekeeper: bk
  };
};

globalThis.createBeekeeperWasmTestFor = async function createBeekeeperWasmTestFor (env) {
  const locBeekeeperWasm = env === 'web' ? '../../dist/bundle/build/beekeeper_wasm.web' : '../../dist/bundle/build/beekeeper_wasm.node';

  const wasm = await import(locBeekeeperWasm);

  const provider = await wasm.default();

  return {
    provider,
    ExtractError,
    BeekeeperInstanceHelper
  };
};

export {};
