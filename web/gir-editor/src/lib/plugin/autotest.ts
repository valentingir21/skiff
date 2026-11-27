// SPDX-License-Identifier: GPL-3.0-only
// (plugin, the tests only) What the plugin's page does in the test host (plugin/tests/host, GIR_PLUGIN_AUTOTEST): a
// scenario, its results as JSON lines (send({type: "test", …}): <out>/results-<instance>.jsonl), a line {step} when the
// host has to act (automate a parameter, close or open the window), {done, ok} at the end. Not loaded otherwise.
//   demo    the virtual FM-1 in the page: the layout lint at the plugin's usual sizes, captures, the DAW's parameters
//   bridge  the virtual FM-1 of the host on CoreMIDI ("Gir Test FM-1") through the plugin's ports: INFO, a Gir
//           parameter, the steps, read back from the unit; DEPTH and KILL automated with the window closed
//   lock    two instances: the second finds the first holding the unit, says where, takes it over; the first hears it
import { plugin, send, request, on } from "./bridge";
import { backup } from "../backup.svelte";
import { device } from "../model/device.svelte";
import { ui } from "../ui.svelte";
import { req } from "../legacy/proto.js";
import { ID } from "../model/tables";
import { killed } from "../kill.svelte";
import { lint } from "../../../tests/e2e/layout-lint";
import type { PluginTransport } from "../transport/plugin";

const TEST_PORT = "Gir Test FM-1";
const sleep = (ms: number) => new Promise((r) => setTimeout(r, ms));
const report = (o: Record<string, unknown>) => send({ type: "test", t: Math.round(performance.now()), ...o });
const step = (s: string, o: Record<string, unknown> = {}) => report({ step: s, ...o });
async function until(f: () => boolean | Promise<boolean>, ms = 15000, every = 100) {
  const end = performance.now() + ms;
  while (performance.now() < end) { if (await f()) return true; await sleep(every); }
  return false;
}
const tp = (k: number, id: number) => device.ask<{ value: number }>(req.trackParam(k, id)).then((r) => r.value);
let failures: string[] = [];
function check(name: string, ok: boolean, more: Record<string, unknown> = {}) {
  report({ check: name, ok, ...more });
  if (!ok) failures.push(name);
}

/** the plugin's page on the test host's virtual FM-1 only (never a unit plugged into the same Mac) */
async function connectTestPort(force = false) {
  device.factory = null;
  const { PluginTransport } = await import("../transport/plugin");
  device.factory = (kind) => {
    const t = new PluginTransport();
    t.only = TEST_PORT;
    if (kind !== "webmidi") throw new Error("test: plugin port only");
    return t;
  };
  try { await device.connect("webmidi", { force }); } finally { device.factory = null; }
}

async function lintViews(size: string) {
  const views = ["sound", "seq", "song", "mixer", "library", "projects", "samples", "fm6", "screen", "settings"];
  let total = 0;
  const rules: Record<string, number> = {};
  const first: string[] = [];
  for (const v of views) {
    ui.view = v as typeof ui.view;
    await sleep(v === "screen" || v === "song" ? 900 : 450);
    const d = lint({ knobSizes: [48, 34], view: v });
    total += d.length;
    for (const x of d) { rules[x.rule] = (rules[x.rule] ?? 0) + 1; if (first.length < 12) first.push(`${v} ${x.rule} ${x.el} @${Math.round(x.x)},${Math.round(x.y)}`); }
  }
  check(`layout lint ${size}`, total === 0, { defects: total, rules, first, vw: innerWidth, vh: innerHeight });
}

async function automation(where: string) {
  // DEPTH: 0.25 of its range on T1; KILL LOW on T2, then off (its value given back); BPM 0.5 (140)
  const d = device.tdesc[ID.P_DEPTH], lo = d?.min ?? 0, hi = d?.max ?? 127;
  const want = Math.round(lo + 0.25 * (hi - lo));
  step("automate", { id: "depth1", value: 0.25 });
  check(`${where}: DEPTH T1 automated reaches the unit`, await until(async () => (await tp(0, ID.P_DEPTH)) === want), { want, got: await tp(0, ID.P_DEPTH) });
  const low = await tp(1, ID.P_EQ_LOW);
  step("automate", { id: "kill2low", value: 1 });
  check(`${where}: KILL LOW T2 on cuts the band`, await until(async () => (await tp(1, ID.P_EQ_LOW)) === 0 && !!killed["1:0"]), { before: low });
  step("automate", { id: "kill2low", value: 0 });
  check(`${where}: KILL LOW T2 off gives it back`, await until(async () => (await tp(1, ID.P_EQ_LOW)) === low && !killed["1:0"]), { got: await tp(1, ID.P_EQ_LOW) });
  step("automate", { id: "bpm", value: 0.5 });
  check(`${where}: BPM automated`, await until(async () => (await device.ask<{ value: number }>(req.get(1, ID.G_BPM))).value === 140));
}

async function demo() {
  await device.connect("emulator");
  check("demo: the virtual FM-1 in the plugin", await until(() => device.ready, 60000), { status: device.status, error: device.error });
  if (!device.ready) return;
  await sleep(1500);
  for (const [w, h] of [[900, 600], [1200, 800], [1440, 900]]) {
    await request({ type: "resize", w, h });
    await sleep(900);
    // (a window as tall as the screen allows: a 900 px tall screen leaves less under the menu bar and the title bar)
    const tallest = screen.availHeight - 40;
    check(`window ${w}x${h}`, Math.abs(innerWidth - w) <= 2 && (Math.abs(innerHeight - h) <= 2 || innerHeight >= Math.min(h, tallest) - 2), { vw: innerWidth, vh: innerHeight, screen: [screen.availWidth, screen.availHeight] });
    await lintViews(`${w}x${h}`);
    for (const v of ["sound", "mixer", "seq"] as const) {
      ui.view = v;
      await sleep(700);
      await request({ type: "snap", name: `plugin-${w}x${h}-${v}` });
    }
  }
  ui.view = "mixer";
  await automation("demo");
}

async function bridge() {
  await connectTestPort();
  check("bridge: INFO through the plugin's ports", device.ready && device.transport?.kind === "plugin", { status: device.status, error: device.error, port: device.portName });
  if (!device.ready) return;
  check("bridge: Gir 1.0 tables", device.tablesMatch === true);
  // a Gir parameter: RUMBLE on a DRUM track
  const k = 3;
  await device.setEngine(k, Math.max(0, device.info!.engines.indexOf("DRUM")));
  await sleep(300);
  const before = await tp(k, ID.P_RB_MIX);
  await device.setParam(k, ID.P_RB_MIX, before + 9);
  check("bridge: RUMBLE set, read back from the unit", await until(async () => (await tp(k, ID.P_RB_MIX)) === before + 9), { before });
  // the steps: one written on track 1, read back as the unit has it
  await device.selectTrack(0);
  const st = await device.ask<{ n: number; notes: number[] }>(req.stepGet(2));
  await device.rq(req.stepSet(2, { ...st, n: 1, notes: [62, 0, 0, 0], time: 0, flags: 0, vel: 100 }));
  const back = await device.ask<{ n: number; notes: number[] }>(req.stepGet(2));
  check("bridge: a step written and read back", back.n === 1 && back.notes[0] === 62, { back });
  // a full backup from the plugin (the checklist's first step): the file written by the plugin (no downloads here)
  const got: { saved: { ok?: boolean; path?: string } | null } = { saved: null };
  const off = on((m) => { if (m.type === "saved") got.saved = m as { ok?: boolean; path?: string }; });
  const okBackup = await backup();
  await until(() => !!got.saved, 20000);
  off();
  const saved = got.saved;
  check("bridge: a full backup, saved by the plugin", okBackup && !!saved?.ok && /Skiff-backup-.*\.json$/.test(saved?.path ?? ""), { saved });
  const s = device.client as unknown as { stats?: unknown };
  report({ stats: device.stats, client: !!s });
  ui.view = "mixer";
  await sleep(500);
  await request({ type: "snap", name: "plugin-bridge-mixer" });
  await automation("window open");
  // the window closed: the page stays, the automation still reaches the unit
  step("close-editor");
  await sleep(1500);
  const d = device.tdesc[ID.P_DEPTH], lo = d?.min ?? 0, hi = d?.max ?? 127, want = Math.round(lo + 0.75 * (hi - lo));
  step("automate", { id: "depth3", value: 0.75 });
  check("window closed: DEPTH T3 automated reaches the unit", await until(async () => (await tp(2, ID.P_DEPTH)) === want, 15000), { want, got: await tp(2, ID.P_DEPTH) });
  // the page's own timers with the window closed (WebKit's throttling off, WebKit.mm): 1 s asked, measured
  await sleep(8000);   // (WebKit throttles a page some seconds after it is hidden)
  const t0 = performance.now();
  await sleep(1000);
  const slept = Math.round(performance.now() - t0);
  check("window closed: the page's timers run on time", slept < 1600, { slept });
  step("open-editor");
  await sleep(1500);
  check("window opened again: still connected, same page", device.ready && device.transport?.kind === "plugin");
  await request({ type: "snap", name: "plugin-bridge-reopened" });
}

async function lock() {
  // (the roles by who comes first: the plugin numbers its instances, and an AU's scan makes one more)
  try { await connectTestPort(); } catch { /* held */ }
  if (device.ready) {
    check("lock: the first instance holds the unit", true);
    // taken over by the second: said, with where
    const ok = await until(() => device.error?.code === "taken", 40000);
    check("lock: the first instance hears it was taken over", ok && /^[^|]*\|[^|]*\|\d+$/.test(device.error?.detail ?? ""), { error: device.error, status: device.status });
    await sleep(800);
    await request({ type: "snap", name: "plugin-lock-taken" });
    return;
  }
  // the second: held by the first (where), then taken over
  const held = device.error?.code === "held";
  const by = device.error?.detail ?? "";
  check("lock: the second instance is told the unit is held, and where", held && /^[^|]*\|[^|]*\|\d+$/.test(by) && !by.endsWith("|" + plugin!.instance), { error: device.error });
  await sleep(500);
  await request({ type: "snap", name: "plugin-lock-held" });
  try { await connectTestPort(true); } catch { /* */ }
  check("lock: taking over connects the second instance", device.ready && (device.transport as PluginTransport | null)?.kind === "plugin", { error: device.error });
  await sleep(3000);   // (the first hears it at its next beat)
}

/** with the desktop app (tests/desktop/lock.spec.ts): this instance holds the unit; the app is told so, then takes over */
async function lockApp() {
  await connectTestPort();
  check("lock with the app: the plugin holds the unit", device.ready, { error: device.error });
  report({ step: "holding" });
  const ok = await until(() => device.error?.code === "taken", 60000);
  check("lock with the app: the plugin hears the app took over", ok && device.error?.detail === "app", { error: device.error });
  await sleep(500);
  await request({ type: "snap", name: "plugin-taken-by-app" });
}

export async function run(name: string) {
  failures = [];
  report({ start: name, instance: plugin?.instance, format: plugin?.format, host: plugin?.host, ua: navigator.userAgent, secure: isSecureContext, audioWorklet: typeof AudioWorkletNode !== "undefined" });
  try {
    if (name === "demo") await demo();
    else if (name === "bridge") await bridge();
    else if (name === "lock") await lock();
    else if (name === "lockapp") await lockApp();
    else check("unknown scenario " + name, false);
  } catch (e) {
    check("exception", false, { error: String((e as Error)?.stack || e) });
  }
  report({ done: true, ok: failures.length === 0, failures });
}
