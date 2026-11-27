// SPDX-License-Identifier: GPL-3.0-only
import { defineConfig } from "vite";
import { svelte } from "@sveltejs/vite-plugin-svelte";
import { readFileSync } from "node:fs";

const pkg = JSON.parse(readFileSync(new URL("package.json", import.meta.url), "utf8"));

// A static site: relative paths, so the same build runs at /skiff/editor/ on GitHub Pages, from file:// in the
// desktop app, and at / in the dev server.
export default defineConfig({
  base: "./",
  plugins: [svelte()],
  define: { __APP_VERSION__: JSON.stringify(pkg.version) },
  server: { port: 5174, fs: { allow: [".."] } },
  build: { target: "es2022", outDir: "dist", assetsInlineLimit: 0, chunkSizeWarningLimit: 1200 },
  test: { include: ["tests/unit/**/*.test.ts"], environment: "node" },
});
