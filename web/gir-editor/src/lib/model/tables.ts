// SPDX-License-Identifier: GPL-3.0-only
// The generated tables (tools/gen_editor_tables.c) and help (tools/gen_editor_help.py), typed, with what the views
// ask of them: a track's pages for its engine, where a parameter is on the device, its CC, a preset's category.
import raw from "../../generated/tables.json";
import helpRaw from "../../generated/help.json";
import type { Desc } from "../protocol/types";

export interface TDesc { label: string; fmt: number; min: number; max: number; def: number; names: string[] | null; unit: string }
export interface Page { title: string; fam: string; scope: string; graph: number; ids: number[]; slots: string[]; send: number; engines: number[]; cond?: string }
export interface Eng { index: number; name: string; ok: number; titles: [string, string]; home: number[]; poly: number; oneshot: number; ownenv: number; cc74: number; cc71: number; edit: TDesc[]; presets: { name: string; cat: string }[] }
export interface Tables {
  version: string; P_COUNT: number; G_COUNT: number; NSTEP: number; P_E0: number; NTRK: number; NENGINES: number;
  ID: Record<string, number>; FAM: string[]; PAGES: Page[]; TP: TDesc[]; GP: TDesc[]; ENG: Eng[]; ORDER: number[];
  CC: [number, number][]; TC_NAME: string[]; LANE_NOTE: number[];
}
export const T = raw as unknown as Tables;
export const ID = T.ID;

export interface HelpPage { what: string; knobs: string[] }
export interface HelpEngine { key?: string; principle?: string; home?: string; voices?: string; uses?: string; gir: boolean; titles: string[]; params: Record<string, string>; doc: string }
export const HELP = helpRaw as unknown as { pages: Record<string, HelpPage>; engines: Record<string, HelpEngine>; sections: Record<string, string>; reference: { where: string; control: string; action: string; effect: string; range: string; cc: string; note: string }[] };

/** the EDIT layer's keys F3 .. F5 for the shown engines (ORDER), G5 INIT */
export const EDIT_KEYS = ["F3", "G3", "A3", "B3", "C4", "D4", "E4", "F4", "G4", "A4", "B4", "C5", "D5", "E5", "F5"];
export const GIR_ENGINES = new Set(["SUPERSAW", "ACID", "MINI", "HOOVER", "INDUS"]);
export const CATEGORIES: Record<string, { en: string; fr: string }> = {
  LD: { en: "Leads", fr: "Leads" }, BS: { en: "Basses", fr: "Basses" }, PA: { en: "Pads", fr: "Nappes" },
  PL: { en: "Plucks", fr: "Plucks" }, ST: { en: "Stabs", fr: "Stabs" }, DR: { en: "Drones", fr: "Drones" },
  FX: { en: "FX", fr: "FX" }, PC: { en: "Percussion", fr: "Percussions" }, KT: { en: "Kits", fr: "Kits" },
  "": { en: "Classic", fr: "Classiques" },
};

/** a table descriptor as the protocol's Desc (scope, id) */
export function tdesc(d: TDesc, scope: number, id: number): Desc {
  return { scope, id, fmt: d.fmt, min: d.min, max: d.max, def: d.def, label: d.label, unit: d.unit, names: d.names || [] };
}

/** the pages a track on engine e has, in the device's order, by family (HOME, SAVE, SEQ excluded: other views) */
export function pagesFor(e: number, opts: { usrSamples?: boolean } = {}): Page[] {
  return T.PAGES.filter((p) => p.engines.includes(e) && (!p.cond || (p.cond === "sample-usr" && opts.usrSamples)));
}

/** "EDIT ×3 › RUMBLE › KNOB 2": how to reach a page's knob on the FM-1 (the family's button pressed n times) */
export function pathOf(page: Page, slot: number, e: number): string {
  const glo = (f: string) => (f === "TRK" ? "GLO" : f);      // GLO's button opens MIXER (the TRK page) first
  const fam = T.PAGES.filter((p) => glo(p.fam) === glo(page.fam) && p.engines.includes(e))
    .sort((a, b) => +(glo(a.fam) === "GLO" && a.fam !== "TRK") - +(glo(b.fam) === "GLO" && b.fam !== "TRK"));
  const n = fam.indexOf(page) + 1;
  const btn = page.fam === "TRK" ? "GLO" : page.fam === "SCL" ? "SEL" : page.fam;
  return `${btn}${n > 1 ? " ×" + n : ""} › ${page.title} › KNOB ${slot + 1}`;
}

/** the MIDI CC that sets parameter id on engine e (the standard map; CC74 / CC71 the engine's brightness / resonance) */
export function ccOf(id: number, e: number): number | null {
  for (const [cc, p] of T.CC) {
    if (p === id) return cc;
    if (p === -74 && T.ENG[e]?.cc74 >= 0 && id === T.P_E0 + T.ENG[e].cc74) return cc;
    if (p === -71 && T.ENG[e]?.cc71 >= 0 && id === T.P_E0 + T.ENG[e].cc71) return cc;
  }
  return null;
}

export const shownEngines = () => T.ORDER.filter((e) => T.ENG[e].ok);
