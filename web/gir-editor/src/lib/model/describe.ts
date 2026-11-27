// SPDX-License-Identifier: GPL-3.0-only
// (rc3) What an engine is and what it is for, and what a preset is, in the interface's language. The engines: the
// manual's overview table (livraison/FINAL/MANUEL-GIR.md §6, "Vue d'ensemble"), in French and English, for all 15 (the
// help generated from the manual has no "principle" line for Gir's five: their sections say it otherwise). The presets:
// Gir's bank, PRESETS.md's "Usage" (src/generated/presets.fr.json, scripts/gen-preset-desc.mjs) and its English
// (presets.en.json). A preset the bank does not describe (Skiff's own) gets its engine's uses.
import presetsFr from "../../generated/presets.fr.json";
import presetsEn from "../../generated/presets.en.json";
import { i18n } from "../i18n/index.svelte";

type Lang = "en" | "fr";
export interface EngineDesc { principle: string; uses: string }
export const ENGINE_DESC: Record<string, Record<Lang, EngineDesc>> = {
  ANALOG: {
    fr: { principle: "Soustractif : 2 oscillateurs, filtre résonant.", uses: "Basses (donk, psy roll, rumble, reese), leads, stabs, plucks, FX." },
    en: { principle: "Subtractive: 2 oscillators, a resonant filter.", uses: "Basses (donk, psy roll, rumble, reese), leads, stabs, plucks, FX." },
  },
  FM6: {
    fr: { principle: "FM à 6 opérateurs, 32 algorithmes, 8 patchs d'usine.", uses: "Plucks FM, cloches, basses métalliques, orgue house." },
    en: { principle: "6-operator FM, 32 algorithms, 8 factory patches.", uses: "FM plucks, bells, metallic basses, house organ." },
  },
  PHASE: {
    fr: { principle: "Distorsion de phase, comme un Casio CZ.", uses: "Stabs et basses « résonantes sans filtre », pads." },
    en: { principle: "Phase distortion, as on a Casio CZ.", uses: "Stabs and basses “resonant without a filter”, pads." },
  },
  SUPERSAW: {
    fr: { principle: "Le supersaw du JP-8000 : 7 scies par voix, un passe-haut qui suit la note, drive et passe-bas résonant.", uses: "Leads et pads trance, stabs rave." },
    en: { principle: "The JP-8000's supersaw: 7 saws a voice, a high-pass that follows the note, drive and a resonant low-pass.", uses: "Trance leads and pads, rave stabs." },
  },
  SAMPLE: {
    fr: { principle: "Instruments échantillonnés : PIANO, FLUTE, SAX, et vos sons USR1 à USR3.", uses: "Pianos, flûtes, sax ; vos samples (vue Samples)." },
    en: { principle: "Sampled instruments: PIANO, FLUTE, SAX, and your own sounds in USR1 to USR3.", uses: "Pianos, flutes, sax; your samples (Samples view)." },
  },
  ACID: {
    fr: { principle: "La voix TB-303 d'ATEK303 : scie ou carré, filtre à diodes, accent et slide par pas.", uses: "Acid, avec l'accent (ACC) et le slide (SLD) du séquenceur." },
    en: { principle: "ATEK303's TB-303 voice: saw or square, a diode filter, accent and slide per step.", uses: "Acid, with the sequencer's accent (ACC) and slide (SLD)." },
  },
  TRIO: {
    fr: { principle: "3 oscillateurs « puce 8 bits », sync, anneau, filtre multimode.", uses: "Stabs d'accord en une touche, chip, cowbell." },
    en: { principle: "3 “8-bit chip” oscillators, sync, ring, a multimode filter.", uses: "One-key chord stabs, chip, cowbell." },
  },
  MINI: {
    fr: { principle: "Un Minimoog, d'après FMini-1 : 3 oscillateurs, filtre en échelle, 128 patchs (SLOT).", uses: "Basses et leads Moog, pads, séquences." },
    en: { principle: "A Minimoog, after FMini-1: 3 oscillators, the ladder filter, 128 patches (SLOT).", uses: "Moog basses and leads, pads, sequences." },
  },
  GRAIN: {
    fr: { principle: "Granulaire sur les échantillons (piano, flûte, sax).", uses: "Nappes, textures, glitch." },
    en: { principle: "Granular, on the samples (piano, flute, sax).", uses: "Pads, textures, glitch." },
  },
  PHYS: {
    fr: { principle: "Modèles physiques : MODAL, STRNG, MEMB, SYMP.", uses: "Percussions, cordes pincées, drones." },
    en: { principle: "Physical models: MODAL, STRNG, MEMB, SYMP.", uses: "Percussion, plucked strings, drones." },
  },
  NOISE: {
    fr: { principle: "Bruit filtré, impulsions, registres à décalage.", uses: "Risers, sweeps, hats, textures." },
    en: { principle: "Filtered noise, impulses, shift registers.", uses: "Risers, sweeps, hats, textures." },
  },
  HOOVER: {
    fr: { principle: "Le hoover rave (Alpha Juno « What The », Mentasm) : PWM désaccordées, sub, plongée de hauteur.", uses: "Hoovers rave, gabber, hardtech." },
    en: { principle: "The rave hoover (Alpha Juno “What The”, Mentasm): detuned PWM, sub, a pitch dive.", uses: "Rave hoovers, gabber, hardtech." },
  },
  DRUM: {
    fr: { principle: "Boîte à rythmes de synthèse, 8 voies, kits STD, 80, 10, 66, 55, 77, DEEP, TIGHT, PERC.", uses: "Kits ; RUMBLE et KICK (Gir) sur le kick." },
    en: { principle: "A synthesised drum machine, 8 lanes, kits STD, 80, 10, 66, 55, 77, DEEP, TIGHT, PERC.", uses: "Kits; RUMBLE and KICK (Gir) on the kick." },
  },
  "DRUM-X": {
    fr: { principle: "Kit de 8 sons de synthèse, chacun avec un patch A et un patch B, et MORPH entre les deux.", uses: "Kits techno, indus, glitch." },
    en: { principle: "A kit of 8 synthesised sounds, each with a patch A and a patch B, and MORPH between them.", uses: "Techno, industrial and glitch kits." },
  },
  INDUS: {
    fr: { principle: "2 sinus, FM, anneau, wavefolder, enveloppe de hauteur, filtre résonant.", uses: "Kicks, percussions métalliques, lasers, drones d'usine." },
    en: { principle: "2 sines, FM, ring, wavefolder, a pitch envelope, a resonant filter.", uses: "Kicks, metallic percussion, lasers, factory drones." },
  },
};

const lang = (): Lang => (i18n.lang === "fr" ? "fr" : "en");
export function engineDesc(name: string): EngineDesc { return ENGINE_DESC[name]?.[lang()] ?? { principle: "", uses: "" }; }
const P: Record<Lang, Record<string, string>> = { fr: presetsFr as Record<string, string>, en: presetsEn as Record<string, string> };
/** a preset's line: the bank's description, else its engine's uses */
export function presetDesc(engine: string, name: string): string {
  const d = P[lang()][`${engine}/${name.trim()}`];
  if (d) return d.charAt(0).toUpperCase() + d.slice(1) + (/[.!?)]$/.test(d) ? "" : ".");
  return engineDesc(engine).uses;
}
export const PRESET_DESC = P;
