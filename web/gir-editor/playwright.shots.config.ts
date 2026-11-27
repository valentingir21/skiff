// SPDX-License-Identifier: GPL-3.0-only
// (rc3) The before / after pictures of the layout report (tests/shots/before-after.spec.ts)
import { defineConfig } from "@playwright/test";
export default defineConfig({ testDir: "tests/shots", timeout: 600_000, workers: 1, reporter: [["list"]],
  use: { launchOptions: { args: ["--autoplay-policy=no-user-gesture-required"] } } });
