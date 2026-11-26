#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Gir Editor's built-in help, from the Gir manual (MANUEL-GIR.md) and the controls reference (REFERENCE-CONTROLES.csv).

    tools/gen_editor_help.py TABLES.json > web/gir-editor/src/generated/help.json

For each page of the device (TABLES.json, tools/gen_editor_tables.c) and each of its knobs: what the manual's tables say
about it. For each engine: its principle, its HOME knobs, its uses (the manual's overview table) and what its EDIT 1 /
EDIT 2 bullets say about each of its eight values. Plus a few whole sections (the rumble, the isolator, the acid
generator) as Markdown, for the editor's "learn more" panels.

The manual lives in the delivery folder beside the repo (../livraison/FINAL, or GIR_MANUAL / GIR_REFERENCE); without
it the committed help.json is kept (tools/gen_editor_tables.sh) and this prints it back unchanged.
"""
import csv
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
FINAL = os.path.join(HERE, "..", "..", "livraison", "FINAL")
MANUAL = os.environ.get("GIR_MANUAL", os.path.join(FINAL, "MANUEL-GIR.md"))
REFERENCE = os.environ.get("GIR_REFERENCE", os.path.join(FINAL, "REFERENCE-CONTROLES.csv"))
COMMITTED = os.path.join(HERE, "..", "web", "gir-editor", "src", "generated", "help.json")

FAMILY_OF_HEADING = {"ENV": "ENV", "LFO": "LFO", "FX": "FX", "SCL": "SCL", "EDIT": "EDIT", "GLO": "GLO", "SAVE": "SAVE",
                     "ARP": "ARP", "SEQ": "SEQ"}


def clean(s):
    """Markdown cell -> plain text (bold, code and links dropped, spaces collapsed)"""
    s = re.sub(r"\*\*(.+?)\*\*", r"\1", s)
    s = re.sub(r"`(.+?)`", r"\1", s)
    s = re.sub(r"\[(.+?)\]\(.+?\)", r"\1", s)
    return re.sub(r"\s+", " ", s).strip(" |")


def cells(line):
    return [c.strip() for c in line.strip().strip("|").split("|")]


def sections(text):
    """{heading: body} for every ### heading (and ## for the chapters)"""
    out, cur, buf = {}, None, []
    for line in text.splitlines():
        m = re.match(r"^(#{2,3}) (.+)$", line)
        if m:
            if cur:
                out[cur] = "\n".join(buf).strip()
            cur, buf = m.group(2).strip(), []
        else:
            buf.append(line)
    if cur:
        out[cur] = "\n".join(buf).strip()
    return out


def page_rows(secs):
    """"FAM/TITLE" -> [KNOB 1 .. KNOB 4] from the 'Page | KNOB 1 | ...' tables of chapter 5 (and the rumble's table)"""
    pages = {}
    for head, body in secs.items():
        m = re.match(r"Famille (\w+)", head)
        fam = FAMILY_OF_HEADING.get(m.group(1)) if m else None
        for line in body.splitlines():
            if not line.startswith("| **"):
                continue
            c = cells(line)
            t = re.match(r"\*\*(.+?)\*\*\s*(\((.*)\))?", c[0])
            if not t or len(c) < 5:
                continue
            title = t.group(1).strip()
            if fam:
                what = re.sub(r"\*\*|[()]", " ", c[0].replace(f"**{title}**", "", 1))
                pages[f"{fam}/{title}"] = {"what": clean(what), "knobs": [clean(x) for x in c[1:5]]}
        if head.startswith("Le RUMBLE"):
            for line in body.splitlines():
                c = cells(line)
                if len(c) == 5 and c[1].startswith("KNOB"):
                    k = int(c[1].split()[1]) - 1
                    p = pages.setdefault(f"EDIT/{c[0]}", {"what": "", "knobs": ["", "", "", ""]})
                    p["knobs"][k] = clean(f"{c[2]} : {c[4]} ({c[3]})")
    return pages


def engine_help(secs, tables):
    names = [e["name"] for e in tables["ENG"] if e["ok"]]
    over = {}
    for line in secs.get("Vue d'ensemble", "").splitlines():
        c = cells(line)
        if len(c) >= 7 and c[1].startswith("**"):
            over[clean(c[1])] = {"key": clean(c[0]), "principle": clean(c[2]), "home": clean(c[3]), "voices": clean(c[4]),
                                 "uses": clean(c[6])}
    out = {}
    for head, body in secs.items():
        m = re.match(r"^([A-Z0-9-]+)( \(Gir\))?( — « (.+?) » / « (.+?) »)?", head)
        if not m or m.group(1) not in names or not m.group(3):
            continue
        name = m.group(1)
        params = {}
        block = re.split(r"\n(?=- EDIT [12])", body)
        for part in block:
            if not part.startswith("- EDIT") and "\n- EDIT" not in part:
                continue
            for mm in re.finditer(r"\*\*([A-Z0-9][A-Z0-9 .+/&#-]*)\*\*\s*([^*\n]*(?:\n  +[^\n*-][^\n*]*)*)", part):
                label, desc = mm.group(1).strip(), clean(mm.group(2)).strip(" ,;:")
                if label not in params and desc:
                    params[label] = desc.lstrip(": ").rstrip(" ;,")
        first = body.split("\n\n")[0]
        out[name] = {**over.get(name, {}), "gir": bool(m.group(2)), "titles": [m.group(4), m.group(5)],
                     "params": params, "doc": body[:4000]}
    return out


def main():
    tables = json.load(open(sys.argv[1]))
    if not os.path.exists(MANUAL):
        sys.stdout.write(open(COMMITTED).read())
        return
    text = open(MANUAL, encoding="utf-8").read()
    secs = sections(text)
    ref = []
    if os.path.exists(REFERENCE):
        with open(REFERENCE, encoding="utf-8") as f:
            for row in csv.reader(f, delimiter=";"):
                if len(row) >= 6 and row[0] != "contexte (page/layer/mode)":
                    ref.append({"where": row[0], "control": row[1], "action": row[2], "effect": row[3], "range": row[4],
                                "cc": row[5], "note": row[6] if len(row) > 6 else ""})
    keep = {k: secs[k] for k in secs if re.match(r"(Le RUMBLE|La page KICK|L'isolateur|Outils \(SEQ|ACID)", k)}
    out = {"source": "livraison/FINAL/MANUEL-GIR.md, REFERENCE-CONTROLES.csv (tools/gen_editor_help.py)",
           "pages": page_rows(secs), "engines": engine_help(secs, tables), "sections": keep, "reference": ref}
    json.dump(out, sys.stdout, ensure_ascii=False, indent=0, sort_keys=True)
    sys.stdout.write("\n")


if __name__ == "__main__":
    main()
