// SPDX-License-Identifier: GPL-3.0-only
// (plugin) The page inside the Gir Editor plugin: what the DAW project keeps (the open view, the chosen track; the
// window's size is the plugin's own), the parameters the DAW automates (the six DEPTH, the KILL LOW / MID / HIGH of
// each track, the BPM), the files the editor saves (into ~/Documents/Gir Editor: WKWebView has no downloads), links
// opened in the browser. The parameters go to the unit as the editor's own controls send them (device.setParam:
// one frame in flight per parameter, the latest value kept); the unit's values come back to the DAW, without being
// sent again. Nothing of the FM-1's content is kept with the project.
import { plugin, on, send, hex } from "./bridge";
import { device } from "../model/device.svelte";
import { ui, go, toastKey, VIEWS, type View } from "../ui.svelte";
import { ID } from "../model/tables";
import { killed, killSet } from "../kill.svelte";

export const pluginInfo = $state({ where: "", track: "" });
const BANDS = ["low", "mid", "high"];
let wantSel: number | null = null;

function applyUi(s: unknown) {
  if (typeof s !== "string" || !s) return;
  try {
    const o = JSON.parse(s) as { view?: View; sel?: number };
    if (o.view && VIEWS.includes(o.view)) go(o.view);
    if (typeof o.sel === "number" && o.sel >= 0 && o.sel < 6) {
      wantSel = o.sel;
      if (device.ready) { device.selectTrack(o.sel); wantSel = null; }
    }
  } catch { /* an older or foreign state: ignored */ }
}

/** a DAW parameter's value (DEPTH 0..1 over its range, KILL 0 / 1, BPM in beats per minute) to the unit */
async function hostParam(id: string, value: number) {
  if (!device.ready) return;
  let m: RegExpMatchArray | null;
  if ((m = /^depth([1-6])$/.exec(id))) {
    const k = +m[1] - 1, d = device.tdesc[ID.P_DEPTH];
    const lo = d?.min ?? 0, hi = d?.max ?? 127;
    await device.setParam(k, ID.P_DEPTH, lo + value * (hi - lo), { undo: false });
  } else if ((m = /^kill([1-6])(low|mid|high)$/.exec(id))) {
    await killSet(+m[1] - 1, BANDS.indexOf(m[2]), value >= 0.5);
  } else if (id === "bpm") {
    await device.setGlobal(ID.G_BPM, Math.round(value), { undo: false });
  }
}

/** the unit's values as the DAW shows them */
function values(): Record<string, number> {
  const v: Record<string, number> = {};
  const d = device.tdesc[ID.P_DEPTH], lo = d?.min ?? 0, hi = d?.max ?? 127;
  device.tracks.forEach((t, k) => {
    v[`depth${k + 1}`] = hi > lo ? (t.p[ID.P_DEPTH] - lo) / (hi - lo) : 0;
    BANDS.forEach((b, i) => { v[`kill${k + 1}${b}`] = killed[`${k}:${i}`] ? 1 : 0; });
  });
  if (device.g[ID.G_BPM] != null) v.bpm = device.g[ID.G_BPM];
  return v;
}

export function initPlugin() {
  if (!plugin) return;
  document.documentElement.classList.add("plugin");
  (globalThis as unknown as { __girSave: (n: string, d: string | Uint8Array) => void }).__girSave = (name, data) =>
    send({ type: "save", name, data: typeof data === "string" ? hex(new TextEncoder().encode(data)) : hex(data) });
  on((m) => {
    if (m.type === "init") { applyUi(m.ui); pluginInfo.where = String(m.where ?? ""); pluginInfo.track = String(m.track ?? ""); }
    else if (m.type === "ui") applyUi(m.state);
    else if (m.type === "track") { pluginInfo.where = String(m.where ?? ""); pluginInfo.track = String(m.name ?? ""); }
    else if (m.type === "param") hostParam(String(m.id), Number(m.value)).catch(() => {});
    else if (m.type === "saved") toastKey(m.ok ? "ok" : "err", m.ok ? "plugin.saved" : "plugin.saveFailed", { path: String(m.path ?? "") });
  });
  $effect.root(() => {
    $effect(() => { send({ type: "ui", state: JSON.stringify({ view: ui.view, sel: device.sel }) }); });
    // the DAW's parameters follow the unit once it is read; before that (and with no unit) nothing is sent either way
    $effect(() => {
      const ready = device.ready;
      send({ type: "live", on: ready });
      if (ready && wantSel != null) { const k = wantSel; wantSel = null; device.selectTrack(k); }
    });
    $effect(() => { if (device.ready) send({ type: "params", values: values() }); });
  });
  send({ type: "ready" });
  if (plugin.autotest) import("./autotest").then((a) => a.run(plugin!.autotest)).catch((e) => send({ type: "test", done: true, ok: false, error: String(e) }));
}
