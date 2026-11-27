// SPDX-License-Identifier: GPL-3.0-only
// (rc2) The interface's two languages are complete: every key in English has its French and the other way round, no
// French left in the English, and every help text the editor shows (generated from the French manual) has its English.
import { describe, expect, it } from "vitest";
import en from "../../src/lib/i18n/en";
import fr from "../../src/lib/i18n/fr";
import help from "../../src/generated/help.json" with { type: "json" };
import helpEn from "../../src/generated/help.en.json" with { type: "json" };
import { isHelp } from "../../src/lib/model/tables";

describe("translations", () => {
  it("English and French have the same keys", () => {
    const a = Object.keys(en).sort(), b = Object.keys(fr).sort();
    expect(a.filter((k) => !(k in fr)), "missing in French").toEqual([]);
    expect(b.filter((k) => !(k in en)), "missing in English").toEqual([]);
  });
  it("no French words in the English", () => {
    const frenchy = /[éèêàçù]|\b(le|la|les|des|une|pour|avec|sur|dans)\b/i;
    const ok = /Français|Hügelton|Gir \(valentingir21\)/;
    const bad = Object.entries(en).filter(([, v]) => frenchy.test(String(v).replace(ok, "")));
    expect(bad).toEqual([]);
  });
  it("the same {placeholders} in both", () => {
    const ph = (s: string) => [...String(s).matchAll(/\{(\w+)\}/g)].map((m) => m[1]).sort().join();
    const bad = Object.keys(en).filter((k) => k in fr && ph((en as any)[k]) !== ph((fr as any)[k]));
    expect(bad).toEqual([]);
  });
  it("every help text shown has its English (help.en.json)", () => {
    const H = help as any, E = helpEn as Record<string, string>, shown: string[] = [];
    for (const p of Object.values<any>(H.pages)) shown.push(...p.knobs);
    for (const e of Object.values<any>(H.engines)) shown.push(e.principle, e.uses, ...Object.values<string>(e.params));
    const missing = [...new Set(shown.filter((s) => s && isHelp(s) && !E[s]))];
    expect(missing).toEqual([]);
  });
});
