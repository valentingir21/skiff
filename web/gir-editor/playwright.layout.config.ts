// SPDX-License-Identifier: GPL-3.0-only
// (rc3) The layout lint (tests/e2e/layout.spec.ts): the whole matrix, 4 browsers at a time
import { defineConfig } from "@playwright/test";
export default defineConfig({
  testDir: "tests/e2e",
  testMatch: /layout\.spec\.ts/,
  timeout: 600_000,
  workers: Number(process.env.LAYOUT_WORKERS || 4),
  fullyParallel: true,
  reporter: [["list"]],
  use: {
    baseURL: process.env.LAYOUT_URL || "http://localhost:5175",
    launchOptions: { args: ["--autoplay-policy=no-user-gesture-required"] },
    acceptDownloads: true,
  },
  webServer: process.env.LAYOUT_URL ? undefined : { command: "npx vite --port 5175 --strictPort", url: "http://localhost:5175", reuseExistingServer: true, timeout: 60_000 },
});
