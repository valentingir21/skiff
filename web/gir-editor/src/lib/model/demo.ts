// SPDX-License-Identifier: GPL-3.0-only
// The demo's first music: on a fresh virtual FM-1, a short techno loop written through the protocol exactly as the
// editor writes anything (sounds, steps, the acid generator by SEQ TOOLS), then saved as project A, "GIR DEMO".
// It shows Gir's own: SUPERSAW, ACID (accent, slide, its generator), MINI, the rumble on a DRUM kick.
import type { Device } from "./device.svelte";
import { TOOL } from "./device.svelte";
import type { Step } from "../protocol/types";
import { ID, T } from "./tables";

const eng = (n: string) => T.ENG.findIndex((e) => e.name === n);
const preset = (e: number, name: string) => Math.max(0, T.ENG[e].presets.findIndex((p) => p.name === name));
const rest = (): Step => ({ n: 0, notes: [0, 0, 0, 0], time: 2, flags: 0, vel: 100, hit: 0, acc: 0, chance: 100, ratchet: 1, flam: 0, cond: 0, nudge: 0, ties: 0 });
const note = (ns: number[], vel = 100, flags = 0): Step => ({ ...rest(), n: ns.length, notes: [...ns, 0, 0, 0, 0].slice(0, 4), time: 0, vel, flags });
const tie = (): Step => ({ ...rest(), time: 1 });
const hits = (hit: number, acc = 0): Step => ({ ...rest(), time: 0, hit, acc });

export async function seedDemo(d: Device, say: (pct: number) => void) {
  const steps = async (k: number, f: (i: number) => Step) => { for (let i = 0; i < 16; i++) await d.writeStep(k, i, f(i)); };
  say(5);
  await d.setGlobal(ID.G_BPM, 132, { undo: false });
  // T1 SUPERSAW: off-beat stabs of Am, then F
  const ss = eng("SUPERSAW");
  await d.loadPreset(0, ss, preset(ss, "ST SS STAB"), { undo: false });
  await steps(0, (i) => (i % 4 === 2 ? note(i < 8 ? [57, 60, 64] : [53, 57, 60], 96) : rest()));
  say(20);
  // T2 ACID: BS 303, a line from Gir's generator (SEQ TOOLS, as by hand)
  const ac = eng("ACID");
  await d.loadPreset(1, ac, preset(ac, "BS 303"), { undo: false });
  await d.loadSteps(1);
  await d.seqTool(1, TOOL.ACID_NEW);
  say(45);
  // T3 MINI: a rolling bass on the off sixteenths
  const mi = eng("MINI");
  await d.loadPreset(2, mi, preset(mi, "BS MOOG HARD"), { undo: false });
  await steps(2, (i) => (i % 4 === 0 ? rest() : i % 4 === 3 ? note([33], 90) : note([45], i % 4 === 1 ? 110 : 80)));
  say(60);
  // T4 DRUM: four on the floor, hats between, a clap on 2 and 4; the rumble rolling DOUBLE under the kick
  const dr = eng("DRUM");
  await d.loadPreset(3, dr, preset(dr, "KT RMB DOUBL"), { undo: false });
  await steps(3, (i) => hits((i % 4 === 0 ? 1 : 0) | (i % 4 === 2 ? 8 : 0) | (i === 4 || i === 12 ? 4 : 0), i === 0 ? 1 : 0));
  await d.setParam(3, ID.P_RB_MIX, 96, { undo: false });
  await d.setParam(3, ID.P_RB_SLICE, 2, { undo: false });
  await d.setParam(3, ID.P_RB_TUNE, 9, { undo: false });
  say(80);
  void tie;
  await d.saveProject(0, "GIR DEMO");
  d.steps = {};
  d.undoStack = []; d.redoStack = [];
  await d.readTracks(true);
  say(100);
}
