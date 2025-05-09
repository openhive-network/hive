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

test.describe('Proper WASM Beekeeper loading on playwright', () => {
  test.beforeEach(async ({ page }) => {
    /// use >> marker for each texts printed in the browser context
    page.on('console', msg => {
      console.log('>>', msg.type(), msg.text());
    });

    await waitForServerReady("http://localhost:5173");
  });

  test('Should properly server-side-render', async () => {
    // Do not goto the page, because we do not want to execute on-side JavaScript

    console.log(`@hiveio/beekeeper version: ${beekeeperPackage.version}`);

    const result = await fetch("http://localhost:5173");
    const text = await result.text();

    expect(text).toContain(beekeeperPackage.version);
  });

  test('WASM should be properly loaded during development', async ({ page }) => {
    await page.goto("http://localhost:5173", { waitUntil: "load" });

    // Wait for wax to be loaded
    await page.waitForFunction(() => typeof (window as any).beekeeperLoaded !== "undefined");

    const result = await page.evaluate(async() => {
      return (window as any).beekeeperLoaded; // This value is set by this app in App.vue after successfull wax initialization
    });

    // Beekeeper should be loaded
    expect(result).toBe(true);
  });
});
