// SPDX-License-Identifier: GPL-3.0-only
// The packaged app (release/mac-universal/… or release/win-unpacked/…): it opens, its window is the app's own origin
// (app://gir), the virtual FM-1 runs the demo, the compact window works. Screenshots go to ci-shots/.
import { test, expect, _electron as electron } from "@playwright/test";
import { existsSync, mkdirSync, readdirSync, rmSync } from "node:fs";
import { join } from "node:path";
import { tmpdir } from "node:os";

const REL = new URL("../../release/", import.meta.url).pathname.replace(/^\/([A-Za-z]:)/, "$1");
const OUT = new URL("../../ci-shots/", import.meta.url).pathname.replace(/^\/([A-Za-z]:)/, "$1");
mkdirSync(OUT, { recursive: true });
function exe(): string {
  if (process.platform === "darwin") {
    for (const d of ["mac-universal", "mac-arm64", "mac"]) {
      const p = join(REL, d, "Gir Editor.app", "Contents", "MacOS", "Gir Editor");
      if (existsSync(p)) return p;
    }
  } else if (process.platform === "win32") {
    const p = join(REL, "win-unpacked", "Gir Editor.exe");
    if (existsSync(p)) return p;
  }
  throw new Error("no packaged app in " + REL + ": " + (existsSync(REL) ? readdirSync(REL).join(", ") : "none"));
}
const plat = process.platform === "darwin" ? "mac" : process.platform === "win32" ? "windows" : process.platform;

test("the desktop app starts, opens the demo, and its compact window", async () => {
  const home = join(tmpdir(), "gir-editor-ci-" + Date.now());   // (a fresh profile: no remembered window)
  rmSync(home, { recursive: true, force: true });
  const app = await electron.launch({ executablePath: exe(), args: [`--user-data-dir=${home}`], env: { ...process.env, ELECTRON_ENABLE_LOGGING: "1" } });
  const win = await app.firstWindow();
  await win.waitForLoadState("domcontentloaded");
  expect(win.url()).toMatch(/^app:\/\/gir\//);
  await expect(win.getByRole("button", { name: /Try without an FM-1|Essayer sans FM-1/ })).toBeVisible({ timeout: 30_000 });
  await win.screenshot({ path: join(OUT, `${plat}-1-welcome.png`) });
  await win.evaluate(() => { localStorage.setItem("gir.lang", JSON.stringify("en")); });
  await win.getByRole("button", { name: /Try without an FM-1|Essayer sans FM-1/ }).click();
  await win.waitForFunction(() => (window as any).__gir?.device.status === "ready", null, { timeout: 60_000 });
  await win.waitForTimeout(1500);
  await win.screenshot({ path: join(OUT, `${plat}-2-demo-sound.png`) });
  const st = await win.evaluate(() => { const d = (window as any).__gir.device; return { tracks: d.tracks.length, ver: d.info.version, desktop: !!(window as any).girDesktop }; });
  expect(st.tracks).toBe(6);
  expect(st.desktop).toBe(true);
  await win.evaluate(() => (window as any).__gir.ui.view = "screen");
  await win.waitForTimeout(1200);
  await win.screenshot({ path: join(OUT, `${plat}-3-demo-screen.png`) });
  // the compact window (the menu's Window › Compact Window): the page reloads in it
  await win.evaluate(() => (window as any).girDesktop.compact(true));
  await win.waitForTimeout(800);
  const w2 = app.windows()[0];
  await w2.waitForLoadState("domcontentloaded");
  await w2.getByRole("button", { name: /Try without an FM-1|Essayer sans FM-1/ }).click().catch(() => {});
  await w2.waitForFunction(() => (window as any).__gir?.device.status === "ready", null, { timeout: 60_000 }).catch(() => {});
  await w2.waitForTimeout(1200);
  await w2.screenshot({ path: join(OUT, `${plat}-4-compact.png`) });
  const remembered = await w2.evaluate(() => (window as any).girDesktop.state());
  expect(remembered.compact).toBe(true);
  await w2.evaluate(() => (window as any).girDesktop.compact(false));
  await app.close();
});
