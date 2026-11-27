// SPDX-License-Identifier: GPL-3.0-only
// (rc2) The packaged desktop app, launched as it is built (electron-builder's unpacked app), on the CI's macOS and
// Windows runners and here: it starts, the demo opens, the screens are kept (the only look at Windows without a PC).
import { defineConfig } from "@playwright/test";
export default defineConfig({ testDir: "tests/desktop", timeout: 120_000, workers: 1, reporter: [["list"]] });
