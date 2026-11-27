// SPDX-License-Identifier: GPL-3.0-only
// (rc2) The FM-1's THEME, in the editor too: the unit's display settings (UI_STATE / UI_SET: THEME, FONT, MONITOR),
// its palettes (UI_PALETTES) and THEME CUSTOM's eleven colours (COLORS, after Felucca 1.6's custom palette and Skiff
// #27), as the original editor's Settings. The editor wears the unit's colours: its ground, surfaces, text and the six
// track colours from the THEME in use (CUSTOM's set when it is the one), as long as the THEME is dark (the editor has
// no light look: a light THEME, PAPER, leaves the editor's own colours).
import { device } from "./model/device.svelte";
import { req, parse, CMD, PAL_BUILTIN, PAL_ROLES, hex565, mix565, luma565, colorWarnings } from "./legacy/proto.js";
import { prefs } from "./store";

export interface Prefs { state: { caps: number; palette: number; font: number; monitor: number; filter: number }; palettes: string[]; colors: { rc: number; stored: boolean; active: boolean; look: number; c: number[] } | null }
export const theme = $state({
  prefs: null as Prefs | null,
  follow: prefs.get("followTheme", true) as boolean,
  /** the colours being edited (CUSTOM), shown in the editor meanwhile */
  edit: null as number[] | null,
});
export const ROLES: string[] = PAL_ROLES;
export const BUILTIN: { name: string; c: number[] }[] = PAL_BUILTIN;
export { hex565, colorWarnings };

export async function readPrefs(fresh = false): Promise<Prefs | null> {
  const info = device.info as any;
  if (!info?.uiCaps || !device.client) return null;
  const state = parse[CMD.UI_STATE](await device.rq(req.uiState(), { retries: 1, quiet: true }));
  const prev = theme.prefs;
  const palettes = prev && !fresh && state.palette < prev.palettes.length ? prev.palettes : parse[CMD.UI_PALETTES](await device.rq(req.uiPalettes(), { retries: 1 }));
  const colors = !(state.caps & 16) ? null : prev?.colors && !fresh && state.palette === prev.state.palette ? prev.colors : parse[CMD.COLORS](await device.rq(req.colors(), { retries: 1 }));
  theme.prefs = { state, palettes, colors };
  apply();
  return theme.prefs;
}

/** the colours of the THEME in use (CUSTOM's set when it is in use and stored) */
export function current(): number[] | null {
  const p = theme.prefs;
  if (!p) return null;
  const name = p.palettes[p.state.palette];
  if (name === "CUSTOM" && p.colors?.stored) return p.colors.c;
  return BUILTIN.find((b) => b.name === name)?.c ?? null;
}

const KEYS = ["--bg", "--bg-2", "--surf", "--surf-2", "--surf-3", "--surf-4", "--line", "--line-2", "--fg", "--fg-2", "--dim", "--mute", "--t1", "--t2", "--t3", "--t4", "--t5", "--t6"];
/** the editor's tokens from a set of eleven colours (as the unit derives its own: mixes from BG / SURF to TEXT) */
export function tokens(c: number[]): Record<string, string> {
  const h = hex565, m = mix565;
  return {
    "--bg": h(c[0]), "--bg-2": h(m(c[0], c[2], 3)), "--surf": h(c[1]), "--surf-2": h(m(c[1], c[2], 6)), "--surf-3": h(m(c[1], c[2], 11)),
    "--surf-4": h(m(c[1], c[2], 17)), "--line": h(m(c[0], c[2], 13)), "--line-2": h(m(c[0], c[2], 21)), "--fg": h(c[2]),
    "--fg-2": h(m(c[0], c[2], 80)), "--dim": h(m(c[0], c[2], 56)), "--mute": h(m(c[0], c[2], 38)),
    ...Object.fromEntries([1, 2, 3, 4, 5, 6].map((k) => [`--t${k}`, h(c[4 + k])])),
  };
}
export const isLight = (c: number[]) => luma565(c[0]) > 128;

export function apply() {
  const s = document.documentElement.style;
  // (SKIFF, the unit's default THEME, is the one the editor's own colours were drawn from: they stay)
  const p = theme.prefs, skiff = !!p && p.palettes[p.state.palette] === "SKIFF";
  const c = theme.edit ?? (theme.follow && !skiff ? current() : null);
  const tk = c && !isLight(c) ? tokens(c) : null;
  for (const k of KEYS) if (tk) s.setProperty(k, tk[k]); else s.removeProperty(k);
}
export function setFollow(on: boolean) { theme.follow = on; prefs.set("followTheme", on); apply(); }

/** THEME, FONT, MONITOR (UI_SET ids 0..2): kept by the unit in its settings (a flash write when stopped) */
export async function setPref(id: number, value: number) {
  const r = parse[CMD.UI_SET](await device.rq(req.uiSet(id, value), { timeout: 4000, retries: 1 }));
  await readPrefs(id === 0);
  return r.rc as number;
}

let last = 0, timer: ReturnType<typeof setTimeout> | null = null;
/** CUSTOM: shown on the unit as it changes (COLORS op 0, RAM only, every 120 ms at most) and in the editor */
export function showColors(look: number, c: number[], live: boolean) {
  theme.edit = c.slice();
  apply();
  if (!live) return;
  if (timer) clearTimeout(timer);
  timer = setTimeout(() => {
    last = Date.now();
    device.rq(req.colors(0, look, c.slice()), { key: "colors" }).catch(() => {});
  }, Math.max(0, 120 - (Date.now() - last)));
}
/** 1 SAVE (THEME CUSTOM from then on), 2 REMOVE the set, 3 drop what is shown */
export async function colorsOp(o: 1 | 2 | 3, look: number, c: number[]) {
  if (timer) clearTimeout(timer);
  const r = parse[CMD.COLORS](await device.writing("colors", () => device.rq(req.colors(o, look, c.slice()), o === 3 ? { retries: 1 } : { timeout: 4000, retries: 1 })));
  theme.edit = null;
  await readPrefs(true);
  return r.rc as number;
}

device.onReady = () => { readPrefs(true).catch(() => {}); };
