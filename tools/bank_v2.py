#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# (techno-presets-v2) The techno bank, second version: one source for the presets the firmware gets (the engines'
# preset tables, MINI's patches, DRUM-X's kits) and for livraison/PRESETS.md.
#   python3 tools/bank_v2.py gen     the tables of firmware/src/eng_*.c from the techno-mixer commit's (BASE) plus
#                                    this bank (replacements in place, the new ones appended): run it again any time
#   python3 tools/bank_v2.py doc OUT the bank, by category, as markdown (moteur, style, usage, notes, référence)
#   python3 tools/bank_v2.py names   the new and reworked presets' names (tools/preset_bench.c's list)
# The sounds' values: tools/bank_v2_spec.py. The references (S01 ..): livraison/RECHERCHE-SONS.md.
import os, re, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BASE = "ddafe37"                                  # techno-mixer: the tables before this bank
FILES = {"ANALOG": "eng_analog.c", "SUPERSAW": "eng_ssaw.c", "ACID": "eng_acid.c", "MINI": "eng_mini.c",
         "HOOVER": "eng_hoover.c", "INDUS": "eng_indus.c", "FM6": "eng_fm6.c", "PHASE": "eng_phase.c",
         "TRIO": "eng_trio.c", "GRAIN": "eng_grain.c", "PHYS": "eng_phys.c", "NOISE": "eng_noise.c",
         "DRUM": "eng_drum.c", "DRUM-X": "eng_drumx.c"}
TABLE = {"ANALOG": "ANALOG_PRESETS", "SUPERSAW": "SSAW_PRESETS", "ACID": "ACID_PRESETS", "MINI": "MINI_PRESETS",
         "HOOVER": "HOOVER_PRESETS", "INDUS": "INDUS_PRESETS", "FM6": "FM6_PRESETS", "PHASE": "PHASE_PRESETS",
         "TRIO": "TRIO_PRESETS", "GRAIN": "GRAIN_PRESETS", "PHYS": "PHYS_PRESETS", "NOISE": "NOISE_PRESETS",
         "DRUM": "DRUM_PRESETS", "DRUM-X": "DRUMX_PRESETS"}
CAT = {"PL": "PT_PLUCK", "ST": "PT_KEYS", "BS": "PT_BASS", "LD": "PT_LEAD", "PA": "PT_PAD", "DR": "PT_PAD",
       "FX": "PT_FX", "PC": "PT_DRUM", "KT": "PT_DRUM"}
ORDER = ["PL", "ST", "BS", "LD", "PA", "DR", "FX", "PC", "KT"]
TITLE = {"PL": "PLUCKS", "ST": "STABS", "BS": "BASSES", "LD": "LEADS", "PA": "PADS", "DR": "DRONES ET TEXTURES",
         "FX": "EFFETS", "PC": "PERCUSSIONS", "KT": "KITS"}
MARK = "    /* (techno-presets-v2) the second techno bank (tools/bank_v2.py, livraison/PRESETS.md) */\n"

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bank_v2_spec as S                          # noqa: E402


def base_text(f):
    return subprocess.check_output(["git", "-C", ROOT, "show", f"{BASE}:firmware/src/{f}"], text=True)


def c_row(p):
    e = ", ".join(str(x) for x in p.e)
    env = ", ".join(str(x) for x in p.env)
    fx = "FX(%d, %d, %d, %d)" % tuple(p.fx)
    more = ""
    if p.more:
        more = ", MORE(" + ", ".join("{P_%s, %d}" % (k, v) for k, v in p.more) + ")"
    return '    {"%s", {%s}, {%s}, %d, %d, %s%s, TAG(%s)},\n' % (p.name, e, env, p.fenv, p.mono, fx, more,
                                                              p.tag or CAT[p.name[:2]])


def table_span(txt, table):
    a = txt.index("static const preset_t %s[] = {" % table)
    b = txt.index("\n};", a) + 1                  # the line of the closing "};"
    return a, b


def gen():
    for eng, f in FILES.items():
        path = os.path.join(ROOT, "firmware/src", f)
        txt = base_text(f)
        a, b = table_span(txt, TABLE[eng])
        body = txt[a:b]
        rows = [p for p in S.BANK if p.eng == eng]
        base_names = re.findall(r'^    \{"([^"]+)"', body, re.M)
        news = []
        for p in rows:
            if p.replace:
                m = re.search(r'^    \{"%s",.*\n' % re.escape(p.replace), body, re.M)
                if not m:
                    sys.exit("%s: no preset %s to replace" % (eng, p.replace))
                body = body[:m.start()] + c_row(p) + body[m.end():]
            else:
                news.append(p)
        body = body + MARK + "".join(c_row(p) for p in news)
        txt = txt[:a] + body + txt[b:]
        if eng == "MINI":                         # each preset's FMini-1 patch, and whether it is a bass
            m = re.search(r"(MINI_PSLOT\[\] = \{\n)(.*?)(\n\};)", txt, re.S)
            old = re.findall(r"\{(\d+), (\d)\}", m.group(2))
            for p in rows:
                if p.replace:
                    old[base_names.index(p.replace)] = (str(p.slot), str(p.bass))
            ent = ["{%s, %s}" % t for t in old] + ["{%d, %d}" % (p.slot, p.bass) for p in news]
            lines, cur = [], "   "
            for x in ent:
                if len(cur) + len(x) + 2 > 118:
                    lines.append(cur.rstrip())
                    cur = "   "
                cur += " " + x + ","
            lines.append(cur.rstrip())
            txt = txt[:m.start(2)] + "\n".join(lines) + txt[m.end(2):]
        if eng == "DRUM-X":                       # each preset's kit (+1; 0 none)
            m = re.search(r"(static const uint8_t DRUMX_PKIT\[\] = \{)([^}]*)(\};)", txt)
            old = [x.strip() for x in m.group(2).split(",")]
            for p in rows:
                if p.replace:
                    old[base_names.index(p.replace)] = str(p.kit)
            new = old + [str(p.kit) for p in news]
            txt = txt[:m.start(2)] + ", ".join(new) + txt[m.end(2):]
        open(path, "w").write(txt)
        print("%-8s %3d presets (%d new, %d reworked)" % (eng, len(base_names) + len(news), len(news),
                                                          len(rows) - len(news)))


def voice(p):
    m = dict(p.more)
    if m.get("VOICE") == 3:
        return "UNI"
    if p.eng in ("DRUM", "DRUM-X"):
        return "KIT"
    return "LEG" if p.mono else "POLY"


def doc(out):
    by = {k: [] for k in ORDER}
    for p in S.BANK:
        if p.name[:2] in by:
            by[p.name[:2]].append(p)
    for k in S.KEPT:
        by[k.name[:2]].append(k)
    ref = {r[0]: r[1] for r in S.REFS}
    L = []
    for c in ORDER:
        if not by[c]:
            continue
        L += ["", "## %s (%d)" % (TITLE[c], len(by[c])), "",
              "| Nom | Moteur | Voix | Style | Usage | Notes | Référence |", "|---|---|---|---|---|---|---|"]
        for p in sorted(by[c], key=lambda p: (p.name)):
            new = "" if getattr(p, "kept", False) else (" *(refait)*" if p.replace else " *(nouveau)*")
            L.append("| **%s**%s | %s | %s | %s | %s | %s | %s %s |" % (
                p.name, new, p.eng, p.voice or voice(p), p.style, p.use, p.notes, p.ref, ref.get(p.ref, "?")))
    open(out, "w").write("\n".join(L) + "\n")
    print("%s: %d sounds" % (out, sum(len(v) for v in by.values())))


if __name__ == "__main__":
    cmd = sys.argv[1] if len(sys.argv) > 1 else ""
    if cmd == "gen":
        gen()
    elif cmd == "doc":
        doc(sys.argv[2])
    elif cmd == "names":
        print("\n".join(p.name for p in S.BANK))
    else:
        sys.exit(__doc__ if False else "usage: bank_v2.py gen | doc OUT | names")
