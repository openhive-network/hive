// This is a workaround for https://github.com/microsoft/playwright/issues/18282#issuecomment-1612266345
import { defineConfig } from '@playwright/test';

export default defineConfig({
  projects: [
    {
      name: "beekeeper_shared_worker_testsuite",
      testDir: "./__tests__"
    }
  ],
  // Run your local dev server before starting the tests
  webServer: {
    command: 'pnpm start'
  }
});
