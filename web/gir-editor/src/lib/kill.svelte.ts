// SPDX-License-Identifier: GPL-3.0-only
// The isolator's KILL (GLO › EQ): a band cut to 0 while held, or locked (Shift), its value before kept to give back.
// The Mixer's buttons and the plugin's KILL parameters (automated in the DAW: locked while on) share it.
import { device } from "./model/device.svelte";
import { ID } from "./model/tables";

export const EQ = [ID.P_EQ_LOW, ID.P_EQ_MID, ID.P_EQ_HIGH];
/** key "k:band": the band's value before the kill, and whether it stays when let go */
export const killed = $state<Record<string, { before: number; locked: boolean }>>({});

export async function killOn(k: number, b: number, lock: boolean) {
  const key = `${k}:${b}`, cur = killed[key];
  if (cur && lock) { await killOff(k, b, true); return; }
  if (cur || !device.tracks[k]) return;
  killed[key] = { before: device.tracks[k].p[EQ[b]], locked: lock };
  await device.setParam(k, EQ[b], 0, { undo: false });
}
export async function killOff(k: number, b: number, force = false) {
  const key = `${k}:${b}`, cur = killed[key];
  if (!cur || (cur.locked && !force)) return;
  delete killed[key];
  await device.setParam(k, EQ[b], cur.before, { undo: false });
}
/** (the plugin's parameter) on: cut and kept; off: given back */
export async function killSet(k: number, b: number, on: boolean) {
  const cur = killed[`${k}:${b}`];
  if (on && !cur) await killOn(k, b, true);
  else if (on && cur) cur.locked = true;
  else if (!on && cur) await killOff(k, b, true);
}
