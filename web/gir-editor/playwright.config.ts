// SPDX-License-Identifier: GPL-3.0-only
import { defineConfig } from "@playwright/test";
export default defineConfig({
  testDir: "tests/e2e",
  timeout: 90_000,
  workers: 1,
  reporter: [["list"]],
  use: {
    baseURL: "http://localhost:5175",
    viewport: { width: 1440, height: 900 },
    launchOptions: { args: ["--autoplay-policy=no-user-gesture-required"] },
    acceptDownloads: true,
  },
  webServer: { command: "npx vite --port 5175 --strictPort", url: "http://localhost:5175", reuseExistingServer: true, timeout: 60_000 },
});
