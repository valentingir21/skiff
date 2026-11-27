// SPDX-License-Identifier: GPL-3.0-only
// (rc3) Every engine is described, in English and French; every preset of Gir's bank too, and every preset of Gir's
// five engines (the Sound view's Engine and Preset cards are never empty)
import { describe, it, expect } from "vitest";
import tables from "../../src/generated/tables.json";
import { ENGINE_DESC, PRESET_DESC } from "../../src/lib/model/describe";

const T = tables as any;
const shown = T.ORDER.filter((e: number) => T.ENG[e].ok).map((e: number) => T.ENG[e]);
describe("descriptions", () => {
  it("each of the 15 engines: what it is and what it is for, in both languages", () => {
    expect(shown.length).toBe(15);
    const missing = shown.flatMap((e: any) => (["en", "fr"] as const).flatMap((l) => (["principle", "uses"] as const)
      .filter((f) => !(ENGINE_DESC[e.name]?.[l]?.[f] ?? "").trim()).map((f) => `${e.name} ${l} ${f}`)));
    expect(missing).toEqual([]);
  });
  it("each preset of the bank in English as in French", () => {
    const fr = Object.keys(PRESET_DESC.fr), en = PRESET_DESC.en;
    expect(fr.length).toBeGreaterThan(400);
    expect(fr.filter((k) => !(en[k] ?? "").trim())).toEqual([]);
  });
  it("each preset of Gir's engines is described", () => {
    const gir = shown.filter((e: any) => ["SUPERSAW", "ACID", "MINI", "HOOVER", "INDUS"].includes(e.name));
    const missing = gir.flatMap((e: any) => e.presets.filter((p: any) => !PRESET_DESC.fr[`${e.name}/${p.name}`] || !PRESET_DESC.en[`${e.name}/${p.name}`]).map((p: any) => `${e.name}/${p.name}`));
    expect(missing).toEqual([]);
  });
});
