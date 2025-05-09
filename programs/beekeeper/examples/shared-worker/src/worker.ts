import type { IBeekeeperInstance } from '@hiveio/beekeeper';

let isBkInitialized = false;
let bkResolve: (value: IBeekeeperInstance) => void;
let bk: Promise<IBeekeeperInstance> = new Promise<IBeekeeperInstance>(resolve => {
  bkResolve = resolve;
});

self.onconnect = event => {
  const port = event.ports[0];

  port.addEventListener("message", async () => {
    if (!isBkInitialized) {
      try { // We import here asyncronously to retrieve any potential loading errors
        isBkInitialized = true;
        const { default: createBeekeeper } = await import('@hiveio/beekeeper/web');
        const instance = await createBeekeeper({ inMemory: true, enableLogs: false });
        bkResolve(instance);
      } catch (error) {
        isBkInitialized = false;
        console.error("Error initializing Beekeeper:", error);
        port.postMessage({ error: `Failed to initialize Beekeeper: "${error}"` });
        return;
      }
    }

    try {
      const instance = await bk;

      port.postMessage({
        version: instance.getVersion()
      });
    } catch (error) {
      console.error("Error processing message:", error);
      port.postMessage({ error: `Failed to process message: "${error}"` });
    }
  });

  port.start();
};
