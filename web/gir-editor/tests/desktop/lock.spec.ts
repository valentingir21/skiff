// SPDX-License-Identifier: GPL-3.0-only
// (plugin) The desktop app and the plugin share one lock on the FM-1 (electron/main.cjs, plugin/src/PortLock.cpp): the
// plugin, in its test host on the host's virtual FM-1, holds it; the app is told so and where (its Connect says it,
// before any port is opened); the app takes over; the plugin hears it. No real FM-1 is opened: the app's side only
// touches the lock (an FM-1 plugged into this Mac stays untouched). Skipped without the plugin's build (plugin/).
import { test, expect, _electron as electron } from "@playwright/test";
import { spawn } from "node:child_process";
import { existsSync, mkdirSync, readFileSync, rmSync } from "node:fs";
import { join } from "node:path";
import { tmpdir } from "node:os";

const ROOT = new URL("../../", import.meta.url).pathname;
const PLUGIN = join(ROOT, "../../plugin/build");
const HOST = join(PLUGIN, "tests/host/girhost_artefacts/Release/girhost.app/Contents/MacOS/girhost");
const VST3 = join(PLUGIN, "GirEditorPlugin_artefacts/Release/VST3/Gir Editor.vst3");
const SHOTS = process.env.GIR_SHOTS || join(ROOT, "../../../livraison/EDITOR/screens/plugin/");

test("the desktop app and the plugin: one holds the FM-1, the other says where and takes over", async () => {
  test.skip(process.platform !== "darwin" || !existsSync(HOST) || !existsSync(VST3), "no plugin build");
  test.setTimeout(150_000);
  const dir = join(tmpdir(), "gir-lock-" + Date.now());
  mkdirSync(dir, { recursive: true });
  mkdirSync(SHOTS, { recursive: true });
  const env = { ...process.env, GIR_LOCK_DIR: dir };
  const host = spawn(HOST, ["--vst3", VST3, "--scenario", "lockapp", "--emu", "--out", dir, "--timeout", "120"], { env, stdio: "pipe" });
  let log = "";
  host.stdout.on("data", (d) => { log += d; });
  const exited = new Promise<number>((res) => host.on("exit", (c) => res(c ?? -1)));
  try {
    await expect.poll(() => /"step": "holding"/.test(log), { timeout: 60_000 }).toBe(true);
    const lock = JSON.parse(readFileSync(join(dir, "port-lock.json"), "utf8"));
    expect(lock.app).toBe("plugin");

    const app = await electron.launch({ args: [ROOT, `--user-data-dir=${join(dir, "profile")}`], env });
    const win = await app.firstWindow();
    await win.evaluate(() => { localStorage.setItem("gir.lang", JSON.stringify("fr")); });
    await win.reload();
    // the app's Connect: held by the plugin, said with where, before any port is opened
    const r = await win.evaluate(() => (window as any).girDesktop.lock(false));
    expect(r).toEqual({ ok: false, by: expect.stringMatching(/\|1$/) });
    await win.evaluate(() => { const d = (window as any).__gir.device; d.error = { code: "held", detail: "Ableton Live 12|Bass|1" }; });
    await expect(win.getByText(/déjà ouvert sur Ableton Live 12, la piste « Bass »/)).toBeVisible();
    await expect(win.getByRole("button", { name: "Prendre la main" })).toBeVisible();
    await win.screenshot({ path: join(SHOTS, "app-held-by-plugin.png") });
    // taking over (the lock only: no port, no unit)
    expect(await win.evaluate(() => (window as any).girDesktop.lock(true))).toEqual({ ok: true });
    expect(await exited).toBe(0);
    expect(log).toMatch(/the plugin hears the app took over", "ok": true/);
    await win.evaluate(() => (window as any).girDesktop.unlock());
    expect(existsSync(join(dir, "port-lock.json"))).toBe(false);
    await app.close();
  } finally {
    host.kill();
    rmSync(dir, { recursive: true, force: true });
  }
});
