import { Page, test as base, expect } from '@playwright/test';

import './globals';
import type { IBeekeeperGlobals, IBeekeeperWasmGlobals, TEnvType } from './globals';

type TBeekeeperTestCallable<R, Args extends any[]> = (gloabls: IBeekeeperGlobals, ...args: Args) => (R | Promise<R>);
type TBeekeeperWasmTestCallable<R, Args extends any[]> = (gloabls: IBeekeeperWasmGlobals, ...args: Args) => (R | Promise<R>);

export interface IBeekeeperTest {
  /**
   * Runs given function in both environments: web and Node.js
   * Created specifically for testing the beekeeper code
   *
   * Checks if results are equal. If your tests may differ please use {@link beekeeperTest.dynamic}
   */
  beekeeperTest: (<R, Args extends any[]>(fn: TBeekeeperTestCallable<R, Args>, ...args: Args) => Promise<R>) & {
    /**
     * Runs given function in both environments: web and Node.js
     *
     * Does not check if results are equal.
     */
    dynamic: <R, Args extends any[]>(fn: TBeekeeperTestCallable<R, Args>, ...args: Args) => Promise<R>;
  };

  /**
   * Runs given function in both environments: web and Node.js
   * Created specifically for testing WASM code
   *
   * Checks if results are equal. If your tests may differ please use {@link beekeeperWasmTest.dynamic}
   */
  beekeeperWasmTest: (<R, Args extends any[]>(fn: TBeekeeperWasmTestCallable<R, Args>, ...args: Args) => Promise<R>) & {
    /**
     * Runs given function in both environments: web and Node.js
     *
     * Does not check if results are equal.
     */
    dynamic: <R, Args extends any[]>(fn: TBeekeeperWasmTestCallable<R, Args>, ...args: Args) => Promise<R>;
  };

  /**
   * Runs given function in web-only environment
   * Created specifically for testing WASM code
   */
  beekeeperWasmTestWebOnly: (<R, Args extends any[]>(fn: TBeekeeperWasmTestCallable<R, Args>, ...args: Args) => Promise<R>);

  /**
   * Runs given function in Node-only environment
   * Created specifically for testing Node code
   */
  beekeeperWasmTestNodeOnly: (<R, Args extends any[]>(fn: TBeekeeperWasmTestCallable<R, Args>, ...args: Args) => Promise<R>);
}

/**
 * @param page Current playwright page
 * @param globalFunction function injecting globals based on the environment
 * @param forceEnv optional parameter for forcing the environment to be used (web or node). If none provided - tests both
 */
const envTestFor = <GlobalType extends IBeekeeperGlobals | IBeekeeperWasmGlobals> (
  page: Page,
  globalFunction: (env: TEnvType) => Promise<GlobalType>,
  forceEnv?: TEnvType
): IBeekeeperTest[GlobalType extends IBeekeeperGlobals ? 'beekeeperTest' : 'beekeeperWasmTest'] => {
  const runner = async <R, Args extends any[]> (checkEqual: Boolean, fn: GlobalType extends IBeekeeperGlobals ? TBeekeeperTestCallable<R, Args> : TBeekeeperWasmTestCallable<R, Args>, ...args: Args): Promise<R> => {
    let nodeData: any, webData: any;

    if (forceEnv === undefined || forceEnv === 'node')
      nodeData = await fn(await (globalFunction as Function)('node'), ...args);


    if (forceEnv === undefined || forceEnv === 'web')
      webData = await page.evaluate(async ({ args, globalFunction, webFn }) => {
        eval(`window.webEvalFn = ${webFn};`);

        return (window as Window & typeof globalThis & { webEvalFn: Function }).webEvalFn(await globalThis[globalFunction]('web'), ...args);
      }, { args, globalFunction: globalFunction.name, webFn: fn.toString() });

    if(typeof nodeData === "object") // Remove prototype data from the node result to match webData
      nodeData = JSON.parse(JSON.stringify(nodeData));

    if(checkEqual)
      expect(webData as any).toStrictEqual(nodeData);

    return webData;
  };

  const using = function<R, Args extends any[]>(fn: GlobalType extends IBeekeeperGlobals ? TBeekeeperTestCallable<R, Args> : TBeekeeperWasmTestCallable<R, Args>, ...args: Args) {
    return runner.bind(undefined, forceEnv === undefined)(fn as any, ...args);
  };
  if (forceEnv === undefined)
    using.dynamic = runner.bind(undefined, false);

  return using as IBeekeeperTest[GlobalType extends IBeekeeperGlobals ? 'beekeeperTest' : 'beekeeperWasmTest'];
};

export const test = base.extend<IBeekeeperTest>({
  beekeeperTest: async({ page }, use) => {
    use(envTestFor(page, createBeekeeperTestFor));
  },
  beekeeperWasmTest: async({ page }, use) => {
    use(envTestFor(page, createBeekeeperWasmTestFor));
  },
  beekeeperWasmTestWebOnly: async({ page }, use) => {
    use(envTestFor(page, createBeekeeperWasmTestFor, 'web'));
  },
  beekeeperWasmTestNodeOnly: async({ page }, use) => {
    use(envTestFor(page, createBeekeeperWasmTestFor, 'node'));
  }
});
