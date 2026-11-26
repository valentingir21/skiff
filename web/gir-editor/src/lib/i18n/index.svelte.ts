// SPDX-License-Identifier: GPL-3.0-only
// The interface's words, English (as the unit) and French. Parameter and page names stay the unit's own.
import { prefs } from "../store";
import en from "./en";
import fr from "./fr";

export type Lang = "en" | "fr";
const DICT: Record<Lang, Record<string, string>> = { en, fr };
const initial = (): Lang => {
  const p = prefs.get<Lang | null>("lang", null);
  if (p === "en" || p === "fr") return p;
  return typeof navigator !== "undefined" && /^fr/i.test(navigator.language) ? "fr" : "en";
};
export const i18n = $state({ lang: initial() });
export function setLang(l: Lang) { i18n.lang = l; prefs.set("lang", l); document.documentElement.lang = l; }

/** t("key", {n: 3}) -> the text in the current language ({n} replaced); the English, then the key, when missing */
export function t(key: string, vars?: Record<string, string | number>): string {
  let s = DICT[i18n.lang][key] ?? DICT.en[key] ?? key;
  if (vars) for (const [k, v] of Object.entries(vars)) s = s.replaceAll(`{${k}}`, String(v));
  return s;
}
