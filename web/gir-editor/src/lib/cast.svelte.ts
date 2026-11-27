// SPDX-License-Identifier: GPL-3.0-only
// (rc2) The screen of a real FM-1, live in the editor: the original editor's Screen tab (web/editor.html, "the Screen
// tab"), the same mechanism, with Gir 1.0 as it is (no firmware change):
//   1. a full backup of the unit (downloaded too, as the restore's), written into an empty copy of the firmware in
//      WebAssembly (felucca_cast.wasm, web/emu/cast_follow.c): its flash is then the unit's;
//   2. CAST op 2: the unit stores its music and restarts; it holds its splash until the cast starts (cast.c);
//   3. back (the editor reconnects as for any unit that comes back): WATCH and CAST op 1 before anything else; from then
//      on the unit sends "cast records" (its inputs, MIDI, frames, each stamped), and the copy replays them, so it draws
//      what the unit draws, with the firmware's own drawing code; every request the editor sends goes into the copy too,
//      which runs it where the unit did (R records).
// Same steps on the virtual FM-1 (its link build casts and restarts like a unit), which the tests use end to end.
import { device } from "./model/device.svelte";
import * as BK from "../../../fm1backup.js";
import { download } from "./ui.svelte";

const { captureBackup } = BK as any;
type Ex = any;
export type CastState = "idle" | "prep" | "back" | "start" | "on";

export const cast = $state({
  state: "idle" as CastState,
  say: "" as string,          // an i18n key
  vars: {} as Record<string, string | number>,
  bad: false,
  pct: 0,                     // the backup's progress
  follow: 100,                // the frames drawn as the unit drew them, %
  frames: 0,
  draws: 0,                   // the copy's screen draws (the view redraws on change)
  leds: [0, 0] as number[],
});

let wasm: ArrayBuffer | null = null;
let copy: Ex | null = null;
let queue: Uint32Array[] = [];
let seq = -1, lost = 0, logging = false;
let pump: ReturnType<typeof setInterval> | null = null;
let timer: ReturnType<typeof setTimeout> | null = null;
let wake: any = null;
const sleep = (ms: number) => new Promise((r) => setTimeout(r, ms));
function say(key: string, vars: Record<string, string | number> = {}, bad = false) { cast.say = key; cast.vars = vars; cast.bad = bad; }

/** what the unit can do: INFO 59 01 bit 0 an FM-1's cast (bit 2 an SMK-37's: not handled here) */
export const canCast = () => !!(device.info && device.info.cast & 1);

async function instance(): Promise<Ex> {
  const { instance } = await WebAssembly.instantiate(wasm!, {});
  (instance.exports as any)._initialize();
  return instance.exports;
}
/** the backup's objects into an empty copy's flash (felucca_web.c web_restore_object); that flash out */
async function flashOf(file: any): Promise<Uint8Array> {
  const a = await instance();
  a.web_nor_erase();
  a.web_boot();
  for (const o of file.objects) {
    const b = Uint8Array.from(atob(o.data), (c) => c.charCodeAt(0));
    new Uint8Array(a.memory.buffer, a.web_obj_buf(), b.length).set(b);
    const rc = a.web_restore_object(o.id, b.length);
    if (rc !== 0) throw new Error(`object ${o.id} (${rc})`);
  }
  return new Uint8Array(a.memory.buffer, a.web_nor(), a.web_nor_size()).slice();
}

export async function startCast() {
  if (cast.state !== "idle" || !device.ready) return;
  if (!canCast()) { say("cast.needFw", {}, true); return; }
  cast.state = "prep";
  try {
    if (!wasm) {
      const base = new URL(import.meta.env.BASE_URL || "./", document.baseURI);
      const r = await fetch(new URL("emu/felucca_cast.wasm", base));
      if (!r.ok) throw new Error("felucca_cast.wasm " + r.status);
      wasm = await r.arrayBuffer();
    }
    await device.stop();
    say("cast.backup", { pct: 0 });
    const file = await captureBackup((r: [number, number[]], o: any) => device.rq(r, { ...o, retries: Math.max(o?.retries ?? 0, 2) }),
      device.info!.version, (d: number, n: number) => say("cast.backup", { pct: Math.floor((100 * d) / Math.max(1, n)) }));
    download(`Skiff-backup-before-screen-${new Date().toISOString().slice(0, 10)}.json`, JSON.stringify(file));
    const nor = await flashOf(file);
    copy = await instance();
    copy.web_nor_erase();
    new Uint8Array(copy.memory.buffer, copy.web_nor(), copy.web_nor_size()).set(nor);
    say("cast.restart");
    device.afterInfo = loaded;
    for (let n = 0; ; n++) {                    // (rc 1: the music not stored yet: asked again)
      let a: number[];
      try { a = await device.rq([83, [2]], { timeout: 2500, retries: 1 }); }
      catch (e) {                               // (its reply lost, and the unit already restarting: its port went)
        if (device.status === "lost" && device.lostWhy === "reboot") break;
        throw e;
      }
      if (a[2] === 0) break;
      if (n >= 30) throw new Error("nostore");
      await sleep(1000);
    }
    cast.state = "back";                        // the unit restarts: its port goes, comes back (Device.reconnect)
    timer = setTimeout(() => { if (cast.state === "back") stopCast("cast.noBack"); }, 25000);
  } catch (e) {
    const m = String((e as Error)?.message || e);
    stopCast(m === "closed" ? "cast.lost" : m === "nostore" ? "cast.noStore" : "cast.failed", { detail: m }, false);
  }
}

/** Device.reconnect, right after INFO: back from the restart, the cast before the editor's own reading (10 s hold) */
async function loaded() {
  device.afterInfo = null;
  if (cast.state !== "back") return;
  if (timer) clearTimeout(timer);
  cast.state = "start";
  say("cast.wait");
  try {
    await device.rq([22, [3]], { front: true, timeout: 500, retries: 3 });   // WATCH (the unit casts only when watched)
    queue = []; seq = -1; lost = 0; logging = false; cast.frames = 0;
    const a = await device.rq([83, [1]], { front: true, timeout: 1000, retries: 2 });
    if (a[1] !== 1) throw new Error("nostart");
    cast.state = "on";
    pump = setInterval(run, 16);
    try { wake = await (navigator as any).wakeLock?.request("screen"); } catch { /* no wake lock */ }
  } catch (e) {
    stopCast("cast.failed", { detail: String((e as Error)?.message || e) }, false);
  }
}

/** every frame the editor sends (the Link's send): after the CAST 1 frame, into the copy, which runs each at its R */
export function castSent(f: number[]) {
  if (cast.state !== "start" && cast.state !== "on") return;
  if (!logging) { logging = f.length === 7 && f[4] === 83 && f[5] === 1; return; }
  if (!copy || f.length > 702) return;
  new Uint8Array(copy.memory.buffer, copy.cf_req_buf(), f.length).set(f);
  copy.cf_req_push(f.length);
}
/** a record (its number, four words of 5 x 7 bits) or the unit's overflow (127) */
export function castRecord(a: number[]) {
  if (cast.state !== "start" && cast.state !== "on") return;
  if (a.length === 1) { stopCast("cast.overflow", {}, false); return; }
  if (seq >= 0 && a[0] !== seq) lost++;
  seq = (a[0] + 1) & 63;
  const w = new Uint32Array(4);
  for (let k = 0; k < 4; k++) {
    const p = 1 + 5 * k;
    w[k] = (a[p] | (a[p + 1] << 7) | (a[p + 2] << 14) | (a[p + 3] << 21) | (a[p + 4] << 28)) >>> 0;
  }
  queue.push(w);
}

let sayAt = 0;
function run() {
  const c = copy;
  if (!c || cast.state !== "on") return;
  while (queue.length) {
    const n = Math.min(64, queue.length), buf = new Uint32Array(c.memory.buffer, c.cf_buf(), 256);
    for (let i = 0; i < n; i++) buf.set(queue[i], 4 * i);
    if (c.cf_push(n) !== n) { stopCast("cast.behind"); return; }
    queue.splice(0, n);
  }
  c.cf_run();
  const d = c.web_screen_draws();
  if (d !== cast.draws) cast.draws = d;
  const l = [c.web_lit_buttons(), c.web_lit_keys()];
  if (l[0] !== cast.leds[0] || l[1] !== cast.leds[1]) cast.leds = l;
  const now = Date.now();
  if (now - sayAt < 500) return;
  sayAt = now;
  const s = (k: number) => c.cf_stat(k), fr = s(1);
  cast.frames = fr;
  cast.follow = fr ? Math.floor((100 * (fr - s(2))) / fr) : 100;
  if (!s(0)) say("cast.wait");
  else if (s(4)) say("cast.out", { n: s(4), m: s(3) }, true);
  else say("cast.follow", { pct: cast.follow });
}

/** the copy's screen (RGB565, the panel's byte order) */
export function castScreen(): Uint16Array | null {
  return copy ? new Uint16Array(copy.memory.buffer, copy.web_screen(), 240 * 240) : null;
}
export function castStat(k: number): number { return copy ? copy.cf_stat(k) : 0; }

export async function stopCast(msg = "", vars: Record<string, string | number> = {}, tell = true) {
  const was = cast.state;
  cast.state = "idle";
  logging = false;
  if (pump) clearInterval(pump);
  if (timer) clearTimeout(timer);
  pump = timer = null;
  if (device.afterInfo === loaded) device.afterInfo = null;
  say(msg, vars, !!msg);
  try { await wake?.release(); } catch { /* gone */ }
  wake = null;
  if (tell && device.ready && (was === "on" || was === "start"))
    await device.rq([83, [0]], { timeout: 1000, retries: 0, quiet: true }).catch(() => {});   // (WATCH's end stops it too)
}

/** the unit went away while the screen was followed (not its own restart) */
export function castLost() {
  if (cast.state === "prep" || cast.state === "start" || cast.state === "on") stopCast("cast.lost", {}, false);
}

device.onCast = castRecord;
device.onSent = castSent;
device.onLost = (why) => { if (why !== "reboot") castLost(); };
