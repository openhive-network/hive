import { expect, test } from '@playwright/test';
import beekeeperPackage from '@hiveio/beekeeper/package.json' with { type: 'json' };

const waitForServerReady = async (url: string, interval: number = 1000, attempts: number = 10): Promise<void> => {
  console.log(`Awaiting a server: ${url}...`);

  for (let count = 0; count < attempts; ++count) {
    try {
      console.log(`Trying to connect ${count}/${attempts})`);

      const response = await fetch(url, {
        method: "GET",
        signal: AbortSignal.timeout(interval)
      });

      if (response.ok && response.status === 200) {
        console.log(`Connected successfully (${count}/${attempts}). Exiting.`);
        return;
      }
    }
    catch (error) {
      if (typeof error !== "object" || !(error instanceof Error))
        console.log(`Caught an UNKNOWN error (${JSON.stringify(error)})`);
      else
      if (error.name === "TimeoutError")
        console.log(`Caught a TIMEOUT error (${JSON.stringify(error)})`);
      else
      if (error.name === "AbortError")
        console.log(`Caught an ABORT error (${JSON.stringify(error)})`);
    }

    console.log(`Waiting for ${interval} ms...)`);
    await new Promise(resolve => { setTimeout(resolve, interval); });
  }

  console.log(`Still down - bailing out.`);
};

test.describe('Proper WASM Beekeeper loading on playwright using SharedWorkers', () => {
  test.beforeEach(async ({ page }) => {
    /// use >> marker for each texts printed in the browser context
    page.on('console', msg => {
      console.log('>>', msg.type(), msg.text());
    });

    await waitForServerReady("http://localhost:5173");

    await page.goto("http://localhost:5173", { waitUntil: "load" });
  });

  test('Should properly load Beekeeper in Shared Worker', async ({ page }) => {
    test.setTimeout(3000); // 3 seconds timeout - we do not need to wait any longer

    console.log(`@hiveio/beekeeper version: ${beekeeperPackage.version}`);

    // Wait for wax to be loaded
    await page.waitForFunction(() => typeof (window as any).worker !== "undefined");

    const result = await page.evaluate(async() => {
      const promise = new Promise(resolve => {
        (window as any).worker.port.onmessage = event => {
          resolve(event.data);
        };
      });

      await (window as any).worker.port.postMessage({});

      return promise;
    });

    expect(result).toStrictEqual({
      version: beekeeperPackage.version
    });
  });
});
