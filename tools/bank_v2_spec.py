# SPDX-License-Identifier: GPL-3.0-only
# (techno-presets-v2) The second techno bank: the sounds (tools/bank_v2.py makes the firmware's tables and PRESETS.md
# of them). Each: engine, name (PL pluck, ST stab, BS bass, LD lead, PA pad, DR drone / texture, FX, PC percussion,
# KT kit), E0..E7, {ATK DEC SUS REL}, ENV -> FILTER, mono (1 = LEG), sends {DIST CHO DLY REV}, up to four more values
# (P_* name, value), then for PRESETS.md: style, usage, notes (the range it is made for), the reference sound of
# livraison/RECHERCHE-SONS.md. replace = the name of the preset it takes the place of (same engine, same number: a
# redundant one, REDONDANCE.md). MINI: slot (FMini-1's patch), bass (its oscillators moved up to the note); DRUM-X: kit
# (DX_KITS + 1: 1 FACTORY 2 DEEP 3 CLICK 4 INDUSTRIAL 5 SOFT 6 GLITCH 7 TRIBAL 8 LASER).
import os, re


class P:
    def __init__(s, eng, name, e, env, fenv, mono, fx, more=(), style="", use="", notes="", ref="", replace=None,
                 slot=0, bass=0, kit=0, voice=None, tag=None):
        assert len(e) == 8 and len(env) == 4 and len(fx) == 4 and len(more) <= 4, name
        assert len(name) <= 12 and (name[:2] in ("PL", "ST", "BS", "LD", "PA", "DR", "FX", "PC", "KT") or
                                    (replace == name and tag)), name   # (a Skiff 2.7 sound retouched: its name, its tag)
        s.tag = tag
        s.eng, s.name, s.e, s.env, s.fenv, s.mono, s.fx, s.more = eng, name, e, env, fenv, mono, fx, list(more)
        s.style, s.use, s.notes, s.ref, s.replace, s.slot, s.bass, s.kit, s.voice = (style, use, notes, ref, replace,
                                                                                    slot, bass, kit, voice)


BANK = []


def mk(eng):
    def f(*a, **k):
        BANK.append(P(eng, *a, **k))
    return f


AN, SS, AC, MI, HV, IN, FM, PH, TR, GR, PY, NZ, DM, DX = (mk(x) for x in (
    "ANALOG", "SUPERSAW", "ACID", "MINI", "HOOVER", "INDUS", "FM6", "PHASE", "TRIO", "GRAIN", "PHYS", "NOISE", "DRUM",
    "DRUM-X"))
UNI = ("VOICE", 3)
# styles
TE, HT, IND, HTR, HTK, HG, MIN, DUB, MEL, DET, ACI, RAV, SCH = ("techno", "hard techno", "techno indus", "hard trance",
                                                              "hardtech", "hardgroove", "minimal", "dub techno",
                                                              "techno mélodique", "Detroit", "acid", "rave 90s",
                                                              "schranz")

# =============================================================================================================== ANALOG
# E: WAVE (0 SAW 1 SQR 2 TRI 3 SIN 4 PWM 5 SYNC 6 SUB) DETUNE MIX NOISE CUTOFF RES DRIVE KEYTRK
AN("PL SAW TECH", (0, 10, 64, 0, 36, 50, 8, 64), (0, 66, 0, 56), 56, 0, (0, 20, 62, 44), (("SPRD", 70),),
   MEL, "le pluck de la techno mélodique : scie, filtre qui claque et se referme, delay", "A3-A5, arp 1/16", "S04",
   replace="LD PLUCK")
AN("PL SQR BLIP", (1, 0, 0, 0, 92, 8, 0, 100), (0, 50, 0, 46), 8, 0, (0, 0, 40, 30), (), MIN,
   "blip carré très court, pour les motifs clairsemés de minimal", "C4-C6, notes isolées", "S05")
AN("PL SINE DROP", (3, 0, 0, 0, 127, 0, 0, 64), (0, 54, 0, 50), 0, 0, (0, 0, 52, 46), (("ED_PIT", 24),), MIN,
   "goutte : sinus court dont la hauteur tombe d'un ton", "C4-C6", "S05")
AN("PL ACID PLK", (0, 0, 0, 0, 34, 112, 44, 72), (0, 58, 0, 40), 52, 0, (28, 0, 58, 18), (("DTYPE", 1),), ACI,
   "pluck acid : résonance au bord du sifflement, drive, delay", "A2-A4, 1/16", "S02")
AN("PL SYNC PLK", (5, 34, 96, 0, 92, 16, 10, 64), (0, 62, 0, 50), 18, 0, (0, 15, 50, 30), (("ED_SHP", 56),), TE,
   "pluck hard-sync : l'enveloppe balaie la scie synchronisée (« piou »)", "A3-A5", "S13")
AN("PL SINE KEYS", (3, 20, 64, 0, 90, 0, 0, 64), (0, 78, 20, 64), 10, 0, (0, 80, 45, 40),
   (('ED_PIT', 4),), DET, "pluck de sinus désaccordés, entre piano électrique et marimba", "A3-A5, accords", "S04")
AN("PL NZ TICK", (2, 0, 0, 112, 104, 60, 0, 127), (0, 42, 0, 40), 30, 0, (0, 0, 50, 35), (), HG,
   "tick bruité accordé (triangle + bruit), entre percussion et note", "C4-C6", "S11")
AN("PL DUB PING", (2, 0, 0, 0, 78, 34, 0, 64), (0, 68, 0, 66), 20, 0, (0, 0, 127, 110), (), DUB,
   "ping de dub techno : note ronde et courte noyée dans le delay et la reverb", "A3-A5, un coup par mesure", "S06")
AN("PL HYPNO", (0, 30, 64, 0, 40, 90, 16, 64), (0, 80, 0, 70), 50, 0, (0, 40, 110, 90),
   (('LRATE', 30), ('LSYNC', 4), ('LD_FLT', 40)), TE, "pluck hypnotique : deux scies désaccordées, longue queue, le filtre ondule sur la demi-mesure",
   "A2-A4, motif répété", "S15")
AN("PL TRI KEY", (2, 6, 64, 0, 96, 10, 0, 90), (0, 74, 10, 64), 0, 0, (0, 34, 40, 52), (), MEL,
   "pluck doux de triangle, presque une note de piano électrique", "A3-A5", "S04")
AN("PL WIDE TRI", (2, 44, 64, 0, 100, 10, 0, 64), (0, 70, 0, 60), 44, 0, (0, 120, 62, 64), (("SPRD", 110),), MEL,
   "pluck très large : deux triangles écartés, SPREAD, gros chorus", "accords A3-A5", "S04")
AN("ST DUB CHORD", (0, 8, 64, 0, 32, 64, 0, 40), (0, 72, 0, 72), 30, 0, (0, 20, 112, 104), (), DUB,
   "accord de dub techno : stab fermé, delay et reverb énormes (à jouer en accord mineur 7)", "accords A2-A4",
   "S21", replace="ST TECHNO")
AN("ST BERLIN", (1, 10, 50, 0, 44, 46, 30, 64), (0, 58, 0, 46), 28, 0, (20, 0, 60, 82), (), TE,
   "stab berlinois : carré sombre, court, reverb sombre", "accords A2-A4", "S21")
AN("ST JUPITER", (0, 12, 64, 0, 66, 8, 0, 64), (56, 72, 92, 56), 30, 0, (0, 44, 40, 40), (('ED_PIT', 10), ('SPRD', 100)), DET,
   "brass Jupiter-8 : attaque lente, la hauteur monte un peu à l'attaque", "accords A3-A5", "S25",
   replace="LD SSAW POLY")
AN("ST DRUMCODE", (0, 18, 64, 0, 50, 82, 40, 64), (0, 60, 0, 44), 46, 0, (30, 10, 55, 45), (("DTYPE", 1),), TE,
   "stab peak-time : scies, filtre résonant qui claque, drive", "accords A2-A4", "S26")
AN("ST FUZZ", (6, 0, 60, 0, 70, 40, 90, 64), (0, 56, 0, 40), 36, 0, (92, 0, 30, 30), (("DTYPE", 4),), HT,
   "stab hard techno : scie + sub dans la FUZZ", "A2-A4", "S28")
AN("ST SYNC", (5, 52, 100, 0, 104, 20, 20, 64), (0, 60, 0, 48), 10, 0, (10, 30, 50, 40), (("ED_SHP", 62),), RAV,
   "stab sync criard qui descend", "accords A3-A5", "S22")
AN("ST DETROIT", (1, 10, 64, 0, 40, 50, 0, 64), (0, 64, 20, 44), 60, 0, (0, 30, 50, 90), (("LRATE", 60),
   ("LD_SHP", 20)), DET, "stab de Detroit : carrés désaccordés, filtre qui claque, reverb", "accords mineurs 7 A3-A5", "S20",
   replace="ST CHORD")
AN("BS DONK", (1, 0, 0, 0, 30, 116, 20, 64), (0, 54, 0, 30), 58, 1, (20, 0, 10, 10), (("ED_PIT", 6),), HG,
   "donk de hard house / bounce : carré, résonance qui « boinge »", "A1-A2, sur les contretemps", "S43",
   replace="BS MINI")
AN("BS DONK UNI", (0, 12, 64, 0, 34, 110, 30, 64), (0, 52, 0, 28), 58, 1, (30, 0, 10, 5), (UNI, ("DETUNE", 50)),
   HG, "donk épais : 8 scies en unisson, même claquement de filtre", "A1-A2, contretemps", "S43")
AN("BS SUB 808", (3, 0, 0, 0, 127, 0, 0, 0), (0, 98, 0, 92), 0, 1, (0, 0, 0, 0), (("ED_PIT", 34),), TE,
   "sub 808 : sinus long, la hauteur tombe à l'attaque", "E1-A1, notes longues", "S45")
AN("BS REESE WID", (0, 127, 64, 0, 64, 10, 20, 40), (0, 64, 127, 30), 6, 1, (70, 110, 0, 10),
   (('LRATE', 100), ('LD_FLT', 6), ('SPRD', 90), ('DTYPE', 4)), HT, "reese large : scies très désaccordées qui battent, chorus",
   "A1-E2, notes tenues", "S41")
AN("BS PWM", (4, 4, 40, 0, 70, 50, 10, 64), (0, 58, 40, 24), 30, 1, (10, 30, 10, 5),
   (('LRATE', 90), ('LD_SHP', 20)), TE, "basse PWM ronde qui bouge", "A1-A2", "S51")
AN("BS SYNC", (5, 30, 100, 0, 72, 20, 40, 64), (0, 60, 80, 22), 24, 1, (30, 0, 5, 5), (("ED_SHP", 40),), HTK,
   "basse sync : attaque qui mord, puis ronde", "A1-A2", "S51", replace="BS ACID 303")
AN("BS OFFBEAT", (6, 0, 40, 0, 60, 20, 10, 64), (0, 54, 0, 26), 30, 1, (0, 0, 10, 5), (), HTR,
   "basse de contretemps : courte, sub + scie (hard trance, hardgroove)", "A1-A2, 1/8 contretemps", "S44")
AN("BS SCHRANZ", (0, 20, 64, 10, 100, 30, 110, 64), (0, 50, 0, 20), 40, 1, (100, 0, 5, 10),
   (("DTYPE", 4), ("DTONE", 20)), SCH, "basse schranz : scie saturée, FUZZ au maximum", "A1-A2", "S47",
   replace="BS ACID HARD")
AN("BS PSY ROLL", (6, 0, 50, 0, 60, 60, 20, 64), (0, 40, 0, 16), 50, 1, (40, 0, 5, 0), (('DTYPE', 1),), HT,
   "basse « psy roll » : très courte, pour les 1/16 roulantes entre les kicks", "A1-A2, 1/16", "S49",
   replace="BS ACID SQR")
AN("BS HARDTEK", (6, 10, 64, 0, 127, 20, 100, 64), (0, 70, 0, 40), 20, 1, (60, 0, 5, 8),
   (('DTYPE', 1), ('ED_PIT', 63)), HTK, "kick-basse hardtek / tribe : sub + scie saturée, gros coup de hauteur à l'attaque",
   "A1-A2", "S48")
AN("BS DETROIT", (1, 8, 64, 0, 48, 20, 10, 64), (0, 66, 60, 30), 24, 1, (0, 20, 10, 10), (), DET,
   "basse de Detroit : carré rond, chaleureux", "A1-A2", "S44")
AN("LD SCREECH", (5, 60, 127, 0, 100, 92, 90, 80), (0, 70, 110, 30), 10, 1, (90, 0, 20, 20),
   (("ED_SHP", 30), ("DTYPE", 1), ("LRATE", 104), ("LD_PIT", 12)), IND,
   "screech industriel : sync résonante saturée, vibrato rapide", "A3-A5", "S60", replace="LD HOOVER")
AN("LD WHISTLE", (3, 0, 0, 20, 127, 0, 0, 64), (10, 70, 120, 50), 0, 1, (0, 20, 60, 60), (('LRATE', 80), ('LD_PIT', 10), ('LFADE', 70), ('GLIDE', 30)),
   DUB, "sifflet : sinus + un souffle, vibrato qui arrive, glide", "A4-A5", "S68", replace="LD ACID")
AN("BS NOISE SUB", (3, 0, 0, 100, 80, 20, 60, 0), (0, 64, 100, 24), 0, 1, (60, 0, 5, 5), (("DTYPE", 4),), IND,
   "sub industriel : sinus + bruit saturés dans la FUZZ", "A1-A2", "S47", replace="BS FUZZ")
AN("LD SQR WOB", (1, 6, 64, 0, 60, 80, 40, 64), (0, 70, 110, 30), 0, 1, (50, 0, 40, 30), (('LRATE', 80), ('LSYNC', 5), ('LD_FLT', 30), ('DTYPE', 1)),
   HT, "lead carré au filtre qui bat à la noire (LFO calé sur le tempo), drive", "A2-A4", "S61", replace="LD HARDTECH")
AN("BS LFO 1/8", (0, 10, 64, 0, 50, 80, 20, 64), (0, 64, 110, 24), 0, 1, (30, 0, 10, 6), (('LRATE', 80), ('LSYNC', 6), ('LWAVE', 2), ('LD_FLT', 40)),
   HT, "basse au filtre balayé en 1/8 par le LFO calé sur le tempo (wobble techno)", "A1-A2, notes tenues", "S51", replace="BS HARDTECH")
TR("SYNC LEAD", (9, 5, 0, 0, 1, 82, 60, 64), (2, 70, 100, 40), 18, 1, (0, 10, 40, 25), (), "Skiff 2.7",
   "son d'origine retouché : sync plus basse, filtre passe-bande", "", "", replace="SYNC LEAD", tag="PT_LEAD")
PH("PD BASS", (0, 0, 70, 80, 4, 0, 100, 0), (0, 60, 60, 25), 0, 1, (5, 0, 5, 10), (), "Skiff 2.7",
   "son d'origine retouché : plus de sub, plus brillant", "", "", replace="PD BASS", tag="PT_BASS")
GR("PIANO HAZE", (0, 20, 100, 80, -12, 50, 20, 70), (70, 90, 120, 100), 0, 0, (0, 50, 30, 90), (), "Skiff 2.7",
   "son d'origine retouché : une octave plus bas, plus sombre", "", "", replace="PIANO HAZE", tag="PT_PAD")
AN("LD SCHRANZ", (0, 30, 64, 0, 80, 50, 100, 64), (0, 70, 110, 30), 20, 1, (127, 20, 15, 20),
   (('VOICE', 3), ('DTYPE', 4), ('LRATE', 88), ('LD_FLT', 30)), SCH, "lead schranz : unisson saturé, filtre qui vibre",
   "A2-A4", "S61", replace="LD GABBER")
AN("LD SIREN", (2, 0, 0, 0, 127, 0, 20, 64), (30, 70, 127, 60), 0, 1, (20, 0, 40, 40),
   (("LRATE", 40), ("LWAVE", 1), ("LD_PIT", 60)), RAV, "sirène rave : la hauteur monte et descend d'une octave",
   "A3-A4, notes longues", "S65")
AN("LD OLDSKOOL", (1, 14, 64, 0, 80, 40, 20, 64), (0, 70, 100, 40), 20, 1, (10, 20, 50, 30), (("GLIDE", 20),),
   TE, "lead techno old school : carrés désaccordés, glide", "A3-A5", "S67", replace="LD TRANCE")
AN("LD ONE NOTE", (0, 0, 0, 0, 30, 100, 30, 64), (0, 72, 110, 40), 0, 1, (20, 0, 60, 30),
   (("M1SRC", 1), ("M1DST", 2), ("M1AMT", 50), ("LRATE", 70)), TE,
   "lead une note : le LFO ouvre et ferme le filtre résonant, on garde la même note", "A2-A3, une note", "S68")
AN("LD SQUEAL", (4, 20, 64, 0, 127, 127, 0, 64), (0, 70, 110, 40), 0, 1, (0, 0, 60, 40),
   (("LRATE", 108), ("LD_PIT", 20), ("LD_SHP", 40)), HTK, "lead criard « moustique » : PWM rapide, vibrato serré",
   "A4-A5", "S69", replace="LD SYNC")
AN("LD PORTA UNI", (0, 24, 64, 0, 60, 70, 10, 64), (2, 70, 115, 50), 40, 1, (0, 40, 55, 60),
   (('VOICE', 3), ('GLIDE', 60), ('DETUNE', 60)), HTR,
   "lead en unisson au glide très long : les notes liées glissent", "A3-A5, notes liées", "S67", replace="LD SSAW UNI")
AN("PA COLD WIND", (1, 20, 64, 40, 36, 40, 0, 40), (110, 90, 120, 105), 0, 0, (0, 30, 20, 120),
   (("LRATE", 12), ("LD_FLT", 16)), TE, "pad froid : carrés filtrés + souffle, très lent", "accords A2-A4", "S81")
AN("PA AMBIENT", (2, 10, 64, 0, 64, 0, 0, 64), (100, 90, 127, 110), 0, 0, (0, 44, 40, 122), (), MEL,
   "pad d'ambient techno : triangles doux, longue reverb", "accords A3-A5", "S85")
AN("PA SWEEP", (0, 16, 64, 0, 30, 62, 0, 64), (80, 90, 120, 100), 0, 0, (0, 60, 30, 100),
   (("LRATE", 8), ("LD_FLT", 50), ("SPRD", 100)), HTR, "pad qui s'ouvre et se ferme en 8 s (filtre résonant)",
   "accords A2-A4", "S80", replace="PA TRANCE")
AN("DR SUB DRONE", (3, 20, 64, 0, 127, 0, 40, 0), (100, 90, 127, 110), 0, 0, (20, 0, 0, 60),
   (("LRATE", 6), ("LD_AMP", 60)), IND, "drone de sub : deux sinus qui battent, saturés, qui respirent",
   "A0-A1, tenu", "S82")
AN("DR TENSION", (0, 127, 64, 30, 50, 70, 20, 64), (110, 90, 127, 110), 0, 0, (10, 40, 20, 110),
   (("LRATE", 18), ("LD_FLT", 14)), IND, "nappe de tension : scies désaccordées au maximum, souffle", "A2-A4",
   "S82")
AN("FX SONAR", (3, 0, 0, 0, 127, 0, 0, 64), (0, 92, 0, 92), 0, 0, (0, 0, 92, 116), (("LRATE", 90), ("LD_PIT", 3)),
   TE, "sonar : sinus aigu qui sonne et se répète au loin", "A4-A6, un coup", "S95")
AN("FX ALARM", (1, 0, 0, 0, 90, 20, 30, 64), (0, 70, 127, 40), 0, 0, (30, 0, 30, 40),
   (("LRATE", 76), ("LWAVE", 3), ("LD_PIT", 40)), IND, "alarme : carré qui saute entre deux notes", "A3-A4, tenu",
   "S97")
AN("FX RISER SAW", (0, 40, 64, 30, 30, 40, 20, 64), (120, 90, 127, 90), 63, 0, (20, 60, 40, 90),
   (UNI, ("ED_PIT", 63)), HTR, "riser : unisson de scies, le filtre et la hauteur montent avec l'attaque (~15 s)",
   "A2-A3, tenu 4-8 mesures", "S90")
AN("FX DROP", (0, 30, 64, 20, 90, 20, 20, 64), (0, 112, 0, 100), 50, 0, (20, 40, 40, 90), (("ED_PIT", 63),), TE,
   "downlifter : la hauteur et le filtre tombent en 3 s", "A3-A4, un coup", "S91")
AN("PC CLAVE", (2, 0, 0, 0, 127, 0, 0, 127), (0, 40, 0, 36), 0, 0, (0, 0, 20, 30), (("ED_PIT", 30),), MIN,
   "clave / tick de minimal (triangle très court, hauteur qui tombe)", "C5-C6", "S119")
AN("PC SNARE ANA", (3, 0, 0, 100, 110, 10, 30, 0), (0, 56, 0, 50), 0, 0, (20, 0, 10, 40), (("ED_PIT", 30),), TE,
   "snare analogique : sinus + bruit", "C3-C4", "S120")

# ============================================================================================================= SUPERSAW
# E: DETUNE MIX CUTOFF RES HPF DRIVE KEYTRK SUB
SS("PL SS TRANCE", (60, 90, 50, 35, 64, 0, 64, 0), (0, 70, 0, 56), 50, 0, (0, 60, 70, 50), (("SPRD", 100),), HTR,
   "pluck supersaw de trance : sept scies, le filtre se referme vite", "A3-A5, arp 1/16", "S03",
   replace="LD SS PLUCK")
SS("LD HARD SAW", (80, 100, 80, 50, 64, 60, 64, 60), (0, 64, 90, 30), 30, 0, (30, 10, 30, 30), (("SPRD", 80), ("DTYPE", 1)),
   HTR, "supersaw dur et serré : drive, sub, sans chorus, release courte", "A3-A5", "S62", replace="LD HARD SAW")
SS("PL SS SHORT", (40, 70, 80, 20, 72, 0, 80, 0), (0, 52, 0, 44), 30, 0, (0, 40, 80, 40), (("SPRD", 90),), HTR,
   "pluck supersaw très court et brillant (gate trance)", "A4-A6, 1/16", "S03")
SS("PL SS DARK", (70, 100, 28, 40, 60, 0, 40, 20), (0, 66, 0, 62), 40, 0, (0, 70, 60, 92), (("SPRD", 110),), MEL,
   "pluck supersaw sombre : fermé, sub, beaucoup de reverb", "A2-A4", "S04")
SS("ST SS HARD", (70, 100, 90, 20, 80, 90, 64, 40), (0, 60, 10, 40), 24, 0, (70, 20, 40, 30),
   (("DTYPE", 1), ("SPRD", 90)), HTK, "stab supersaw saturé (hardtech), avec sub", "accords A2-A4", "S28")
SS("LD SS SKY", (50, 90, 70, 60, 64, 0, 110, 0), (10, 74, 110, 70), 10, 0, (0, 70, 55, 65),
   (("SPRD", 110), ("LRATE", 92), ("LD_PIT", 10), ("LFADE", 80)), HTR,
   "lead trance plus doux, vibrato qui arrive après la note", "A3-A5", "S62", replace="LD SS TRANCE")
SS("LD SS SCREAM", (80, 110, 96, 80, 100, 110, 64, 0), (0, 70, 115, 40), 0, 0, (90, 30, 30, 30),
   (("DTYPE", 4), ("SPRD", 80)), HTK, "supersaw résonant dans la FUZZ, coupé dans le bas", "A3-A5", "S64",
   replace="LD SS HTECH")
SS("LD SS ANTHEM", (127, 127, 118, 10, 64, 10, 64, 20), (30, 76, 115, 76), 4, 0, (0, 120, 70, 100),
   (("SPRD", 127),), HTR, "supersaw « anthem » au désaccord maximal, attaque douce, hall énorme", "accords A3-A5",
   "S62", replace="LD SS ANTHEM")
SS("PA SS SWELL", (90, 110, 70, 10, 70, 0, 50, 0), (112, 90, 120, 108), 30, 0, (0, 110, 30, 115),
   (("SPRD", 127), ("LRATE", 14), ("LD_SHP", 30)), HTR, "nappe trance qui gonfle lentement, le désaccord respire",
   "accords A2-A4", "S80")
SS("BS SS OFFBT", (30, 60, 90, 50, 32, 60, 64, 127), (0, 52, 0, 24), 30, 1, (20, 0, 10, 5), (), HTR,
   "basse supersaw de contretemps : courte, sub au maximum", "A1-A2, 1/8 contretemps", "S44",
   replace="BS SUPERSAW")
SS("DR SS TENSE", (127, 127, 40, 60, 90, 40, 30, 60), (120, 90, 127, 120), 10, 0, (20, 60, 20, 120),
   (("SPRD", 127), ("LRATE", 4), ("LD_FLT", 30)), IND, "nappe de tension : supersaw fermé et résonant, sub, lent",
   "A1-A3", "S82")

# ================================================================================================================= ACID
# E: WAVE (0 SAW 1 SQR 2 SAW+SUB 3 SQR+SUB) CUTOFF RES ENV MOD DECAY ACCENT TUNE SLIDE
AC("BS 303 TEAR", (0, 100, 100, 100, 20, 120, 0, 30), (0, 100, 127, 10), 0, 1, (127, 0, 20, 8),
   (("DTYPE", 4), ("DTONE", 16)), ACI, "303 déchiré : FUZZ, filtre plus ouvert, décroissance courte", "A1-A2",
   "S42", replace="BS 303 HARD")
AC("BS 303 SQLCH", (1, 30, 127, 127, 20, 127, 0, 45), (0, 100, 127, 10), 0, 1, (10, 0, 30, 10), (), ACI,
   "303 squelch en carré : résonance au maximum, accents violents", "A1-A2", "S42", replace="BS 303 SQLCH")
AC("BS PHUTURE", (0, 10, 112, 127, 127, 95, 0, 60), (0, 100, 127, 14), 0, 1, (10, 0, 90, 40), (), ACI,
   "style Phuture « Acid Tracks » : filtre fermé, énorme balayage, longue décroissance", "A1-A2", "S42",
   replace="BS PHUTURE")
AC("PL 303", (0, 80, 120, 30, 20, 90, 0, 20), (0, 100, 127, 8), 0, 1, (0, 0, 90, 40), (), ACI,
   "pluck 303 : décroissance très courte, delay", "A2-A4, 1/16", "S02")
AC("LD 303 SCRM", (1, 90, 127, 90, 60, 110, 0, 50), (0, 100, 127, 20), 0, 1, (60, 0, 90, 30),
   (("DTYPE", 1),), ACI, "lead acid hurlant : 303 dans l'aigu, résonance et drive HARD", "A2-A4", "S66")

# ================================================================================================================= MINI
# E: CUTOFF EMPH CONTOUR F.ATK F.DEC F.SUS MOD (SLOT: slot=)
MI("BS FEEL", (40, 40, 50, 0, 60, 20, 0, 0), (0, 64, 60, 20), 0, 1, (0, 0, 50, 10), (), TE,
   "basse séquencée « I Feel Love » (patch I FEEL ARP de FMini-1) : courte, delay", "A1-A2, 1/16", "S50",
   slot=103, bass=1, replace="BS MOOG OCT")
MI("PL RAVE ARP", (66, 60, 60, 0, 44, 0, 0, 0), (0, 58, 0, 46), 0, 0, (10, 40, 60, 40), (), RAV,
   "arp rave (patch RAVE ARP) : carré nasal et court", "A3-A5", "S12", slot=14)
MI("PL SAW MOOG", (30, 100, 90, 0, 40, 0, 0, 0), (0, 60, 0, 48), 0, 0, (10, 20, 50, 30), (), TE,
   "pluck scie Moog (patch PLUCK SAW), emphasis haute", "A2-A4", "S04", slot=60)
MI("ST MG BRASS", (90, 60, 60, 20, 60, 50, 0, 0), (0, 62, 0, 48), 0, 0, (0, 40, 30, 40), (), DET,
   "stab brass Moog (patch BRASS STAB)", "accords A3-A5", "S25", slot=54)
MI("ST FARFISA", (110, 40, 0, 0, 60, 127, 0, 0), (0, 56, 30, 30), 0, 0, (0, 30, 40, 40), (), HG,
   "stab d'orgue Farfisa (patch FARFISA) : orgue nasal court", "accords A3-A5", "S23", slot=79)
MI("ST FIFTHS", (40, 80, 50, 0, 70, 0, 0, 0), (0, 70, 0, 60), 0, 0, (10, 30, 60, 90), (), TE,
   "stab de quintes (patch FIFTHS) : une touche = une quinte", "A2-A4", "S27", slot=13)
MI("BS WOBBLE", (40, 60, 60, 0, 70, 40, 50, 0), (0, 70, 110, 30), 0, 1, (30, 0, 10, 10), (), HT,
   "basse wobble (patch WOBBLE BASS) : le filtre bat", "A1-A2", "S51", slot=33, bass=1)
MI("BS RUBBER", (60, 80, 70, 0, 50, 10, 0, 0), (0, 60, 40, 24), 0, 1, (10, 0, 10, 10), (('GLIDE', 60),), TE,
   "basse caoutchouc (patch RUBBER), glide", "A1-A2", "S50", slot=37, bass=1)
MI("BS GROWL MG", (30, 90, 90, 0, 50, 30, 40, 0), (0, 70, 110, 24), 0, 1, (60, 0, 5, 5), (("DTYPE", 4),), HT,
   "basse growl (patch GROWL) : le filtre gronde et module", "A1-A2", "S47", slot=19, bass=1)
MI("BS SEQ MOOG", (50, 50, 64, 0, 46, 0, 0, 0), (0, 56, 0, 22), 0, 1, (0, 0, 20, 5), (), TE,
   "basse de séquence (patch SEQ BASS), très courte", "A1-A2, 1/16", "S44", slot=35, bass=1)
MI("LD SIREN LD", (70, 40, 20, 0, 70, 80, 0, 0), (0, 70, 120, 50), 0, 1, (20, 0, 40, 40), (('LRATE', 50), ('LD_PIT', 30)), RAV,
   "lead sirène (patch SIREN LEAD)", "A3-A5", "S65", slot=118)
MI("LD MOSQUITO", (90, 70, 20, 0, 70, 100, 0, 0), (0, 70, 120, 40), 0, 1, (40, 0, 30, 30), (), HTK,
   "lead moustique (patch MOSQUITO) : aigu, nasal, criard", "A4-A5", "S69", slot=119)
MI("LD MG PORTA", (40, 80, 100, 60, 60, 60, 0, 0), (60, 70, 110, 40), 0, 1, (20, 0, 40, 30), (('GLIDE', 60),), TE,
   "lead sync Moog à attaque lente et glide long (patch MONO SYNC)", "A3-A5", "S67", slot=108)
MI("LD MOOG", (61, 44, 44, 0, 79, 100, 30, 0), (5, 81, 110, 60), 0, 1, (0, 40, 70, 50), (("GLIDE", 20), ("LRATE", 84), ("LD_PIT", 8), ("LFADE", 70)), TE,
   "lead Minimoog mono, glide, vibrato qui arrive, delay", "A3-A5", "S67", slot=0, replace="LD MOOG")
AN("NEON LEAD", (2, 14, 64, 0, 92, 25, 12, 64), (6, 70, 110, 55), 14, 1, (0, 40, 55, 40), (("LRATE", 86), ("LFADE", 92), ("LD_PIT", 11), ("GLIDE", 28)),
   "Skiff 2.7", "son d'origine retouché : triangle au lieu de la scie, plus de chorus", "", "", replace="NEON LEAD", tag="PT_LEAD")
AN("BS ROLLING", (1, 8, 64, 0, 50, 60, 20, 64), (0, 50, 0, 18), 50, 1, (20, 0, 15, 5), (), TE,
   "basse roulante en 1/16 entre les kicks, courte et claquante", "A1-A2, 1/16", "S44", replace="BS ROLLING")
MI("LD MOOG TRNC", (54, 56, 47, 0, 81, 50, 30, 0), (10, 84, 115, 70), 0, 1, (0, 90, 80, 70), (("GLIDE", 18), ("LRATE", 30), ("LD_FLT", 20)), HTR,
   "lead trance : mur de 3 scies désaccordées du Moog (patch SAW WALL), le filtre respire, gros chorus", "A3-A5", "S62", slot=112, replace="LD MOOG TRNC")
MI("LD BIG HOOK", (100, 20, 20, 0, 70, 100, 0, 0), (2, 70, 115, 50), 0, 1, (10, 100, 50, 50), (), HTR,
   "gros lead de refrain (patch BIG HOOK)", "A3-A5", "S62", slot=114)
MI("PA SOLINA", (70, 10, 20, 40, 80, 100, 0, 0), (90, 90, 120, 100), 0, 0, (0, 100, 30, 90), (), DET,
   "strings Solina (patch SOLINA) : les cordes de Detroit", "accords A3-A5", "S84", slot=40)
MI("PA JUNO PWM", (30, 20, 30, 100, 80, 90, 0, 0), (80, 90, 120, 96), 0, 0, (0, 90, 30, 100), (), TE,
   "pad PWM façon Juno (patch JUNO PWM)", "accords A2-A4", "S88", slot=42)
MI("PA ECLIPSE", (40, 50, 60, 90, 90, 60, 0, 0), (100, 90, 120, 110), 0, 0, (0, 60, 30, 115), (), IND,
   "pad sombre qui s'ouvre (patch ECLIPSE)", "accords A2-A4", "S81", slot=46)
MI("DR IRON", (50, 60, 40, 0, 80, 80, 0, 0), (110, 90, 127, 110), 0, 0, (30, 40, 20, 110), (), IND,
   "drone de fer (patch IRON DRONE)", "A1-A3, tenu", "S82", slot=86)
MI("DR DARK MOOG", (30, 50, 30, 0, 90, 80, 0, 0), (110, 90, 127, 110), 0, 0, (10, 40, 20, 115), (), TE,
   "drone sombre (patch DARK DRONE)", "A1-A3, tenu", "S82", slot=80)
MI("FX LASER MG", (90, 60, 100, 0, 40, 0, 0, 0), (0, 70, 0, 60), 0, 0, (10, 0, 50, 50), (), RAV,
   "laser (patch LASER ZAP)", "A3-A5, un coup", "S93", slot=96)
MI("FX JET", (60, 40, 60, 100, 100, 60, 0, 0), (60, 90, 127, 90), 0, 0, (10, 40, 30, 90), (), HTR,
   "réacteur qui passe (patch JET SWEEP)", "A2-A4, tenu", "S92", slot=90)
MI("FX POWER UP", (60, 40, 70, 0, 80, 60, 0, 0), (0, 80, 60, 70), 0, 0, (0, 20, 50, 60), (), RAV,
   "montée « power up » (patch POWER UP)", "A3-A4, un coup", "S90", slot=95)
MI("FX UFO", (60, 60, 40, 0, 70, 80, 0, 0), (40, 90, 127, 90), 0, 0, (0, 40, 50, 90), (), IND,
   "soucoupe (patch UFO) : tremblement de hauteur", "A3-A5, tenu", "S96", slot=9)

# =============================================================================================================== HOOVER
# E: DIVE D.TIME PWM DETUNE CUTOFF RES SUB DRIVE
HV("LD GABBER HV", (110, 80, 110, 90, 112, 18, 70, 120), (0, 70, 115, 40), 0, 1, (127, 0, 15, 20),
   (('DTYPE', 4),), HTK, "hoover gabber : plongée de deux octaves, désaccord énorme, FUZZ", "A2-A4",
   "S63", replace="LD GABBER HV")
HV("LD HOOV HARD", (90, 40, 127, 60, 127, 20, 127, 80), (0, 70, 115, 45), 0, 1, (90, 0, 20, 30), (('DTYPE', 1), ('LRATE', 80), ('LSYNC', 8), ('LD_AMP', 100)),
   HTK, "hoover hardtech haché en 1/16 (LFO calé sur le tempo), saturé, sans chorus", "A2-A4", "S63", replace="LD HOOV HARD")
AN("PA GATE SQR", (1, 12, 64, 0, 70, 30, 0, 64), (20, 80, 120, 60), 0, 0, (0, 60, 40, 80), (("LRATE", 80), ("LSYNC", 6), ("LWAVE", 3), ("LD_AMP", 127)),
   HTR, "pad gaté en 1/8 : carrés hachés par le LFO synchronisé au tempo", "accords A3-A5", "S86", replace="PA SSAW")
TR("TURBO SYNC", (9, 14, 0, 3, 0, 110, 28, 64), (2, 72, 105, 45), 20, 1, (6, 15, 55, 30), (("GLIDE", 24), ("LRATE", 86), ("LFADE", 90), ("LD_PIT", 8)),
   "Skiff 2.7", "son d'origine retouché : sync plus haute, plus brillant", "", "", replace="TURBO SYNC", tag="PT_LEAD")
HV("LD HOOV TRNC", (16, 50, 70, 24, 116, 10, 20, 20), (8, 72, 112, 64), 0, 0, (10, 100, 80, 80),
   (('SPRD', 120), ('LRATE', 90), ('LD_PIT', 16), ('LFADE', 80)), HTR,
   "hoover de hard trance : plongée courte, brillant, vibrato qui arrive", "accords A3-A5", "S63",
   replace="LD HOOV TRNC")
HV("LD HOOV ACID", (20, 60, 80, 30, 40, 110, 40, 60), (0, 66, 60, 30), 50, 1, (30, 10, 40, 30), (('SPRD', 80), ('DTYPE', 1)),
   HTK, "hoover acid : filtre résonant qui claque, petite plongée", "A2-A4", "S66")
HV("ST HOOV DIVE", (127, 70, 90, 50, 100, 10, 30, 40), (0, 70, 0, 60), 0, 0, (80, 90, 90, 60), (("SPRD", 120),),
   RAV, "stab hoover qui plonge de presque deux octaves", "accords A3-A5", "S24")
HV("BS HOOV DIVE", (100, 100, 60, 40, 100, 20, 60, 60), (0, 70, 60, 30), 0, 1, (30, 40, 10, 5), (('DTYPE', 0),), HTK,
   "basse hoover : chaque note plonge, sub", "A1-A2", "S51")
HV("PA HOOVER", (0, 0, 127, 30, 80, 10, 20, 0), (90, 90, 120, 100), 0, 0, (0, 110, 30, 100),
   (("SPRD", 127), ("LRATE", 20), ("LD_FLT", 10)), RAV, "nappe hoover : PWM chatoyante, sans plongée",
   "accords A2-A4", "S80")
HV("FX HOOV DIVE", (127, 112, 127, 80, 100, 20, 60, 60), (0, 100, 60, 90), 0, 0, (40, 80, 40, 90), (), HTK,
   "plongée géante : deux octaves en ~3 s", "A2-A4, un coup", "S91")
HV("PL HOOVER", (40, 30, 90, 40, 90, 10, 20, 20), (0, 60, 0, 50), 0, 0, (10, 90, 60, 50), (("SPRD", 120),), RAV,
   "pluck hoover : petite plongée, très court, chorus", "A3-A5", "S24")

# ================================================================================================================ INDUS
# E: RATIO MOD RING NOISE FOLD P.ENV P.DECAY RES (+ CUTOFF: P_IN_CUT)
IN("PC KICK SINE", (64, 0, 0, 0, 10, 70, 52, 0), (0, 70, 0, 60), 0, 0, (0, 0, 0, 10), (("IN_CUT", 90),), TE,
   "kick de synthèse : sinus, la hauteur tombe de presque 3 octaves", "C1-C2", "S110")
IN("PC KICK GABR", (64, 10, 0, 0, 110, 80, 56, 0), (0, 72, 0, 56), 0, 0, (120, 0, 0, 20),
   (("IN_CUT", 110), ("DTYPE", 1)), HTK, "kick gabber : sinus replié et saturé jusqu'à la distorsion", "C1-C2",
   "S115")
IN("PC KICK RMBL", (64, 0, 0, 10, 40, 60, 60, 0), (0, 80, 0, 80), 0, 0, (60, 0, 20, 120),
   (("IN_CUT", 60), ("DTYPE", 0)), HT, "kick + rumble : saturé, reverb énorme filtrée", "C1-C2", "S40")
IN("FX LASER", (64, 0, 0, 0, 20, 127, 60, 0), (0, 64, 0, 56), 0, 0, (10, 0, 60, 50), (("IN_CUT", 127),), RAV,
   "laser : sinus qui tombe de 4 octaves en un éclair", "A4-A6", "S93")
IN("FX ZAP IND", (90, 60, 0, 0, 40, 100, 48, 0), (0, 58, 0, 50), 0, 0, (20, 0, 50, 40), (("IN_CUT", 127),), DET,
   "zap des débuts de la techno : FM qui tombe vite", "A3-A5", "S96")
IN("FX CLANK", (113, 120, 40, 20, 60, 40, 40, 100), (0, 60, 0, 56), 0, 0, (30, 0, 30, 80), (("IN_CUT", 120),), IND,
   "clank métallique : FM inharmonique, anneau, résonance", "A2-A4", "S98")
IN("FX ROBOT", (40, 30, 127, 0, 40, 0, 60, 0), (0, 70, 127, 50), 0, 0, (20, 0, 40, 40),
   (("IN_CUT", 110), ("LRATE", 88), ("LD_SHP", 40)), IND, "voix de robot : modulation en anneau qui grésille",
   "A2-A4, tenu", "S98")
IN("BS INDUS", (20, 60, 0, 0, 70, 10, 40, 30), (0, 64, 80, 24), 0, 1, (40, 0, 5, 10), (("IN_CUT", 80),), IND,
   "basse FM métallique : rapport 1:2, un peu replié", "A1-A2", "S46")
IN("BS IND FOLD", (64, 30, 0, 0, 110, 10, 30, 0), (0, 64, 100, 20), 0, 1, (60, 0, 5, 5), (("IN_CUT", 70),), HT,
   "basse sinus repliée : grondement agressif", "A1-A2", "S47")
IN("BS KICKBASS", (64, 20, 0, 0, 100, 90, 50, 0), (0, 72, 0, 50), 0, 1, (80, 0, 5, 10),
   (("IN_CUT", 90), ("DTYPE", 1)), HTK, "kick-basse hardtek : coup de hauteur, puis basse saturée", "A1, F1",
   "S48")
IN("LD IND RING", (52, 20, 100, 0, 60, 0, 60, 40), (0, 70, 115, 40), 0, 1, (40, 0, 30, 30),
   (("IN_CUT", 120), ("GLIDE", 20)), IND, "lead en anneau, métallique et creux, résonance", "A3-A5", "S60")
IN("LD FOLD SCRM", (64, 40, 0, 0, 127, 0, 60, 90), (0, 70, 115, 40), 0, 1, (90, 0, 20, 20),
   (("IN_CUT", 127), ("LRATE", 100), ("LD_PIT", 10)), IND, "screech de sinus replié au maximum, sifflement",
   "A3-A5", "S60")
IN("PL INDUS", (50, 60, 60, 0, 0, 0, 40, 40), (0, 60, 0, 56), 0, 0, (10, 0, 60, 50), (('IN_CUT', 90),), IND,
   "pluck métallique : FM + anneau, court, delay", "A3-A5", "S09")
IN("PL TIN", (77, 0, 110, 0, 20, 10, 40, 60), (0, 54, 0, 48), 0, 0, (0, 0, 50, 40), (("IN_CUT", 127),), MIN,
   "pluck de fer-blanc : anneau, résonance", "A3-A5", "S09")
IN("PA INDUS", (85, 40, 30, 20, 30, 0, 60, 30), (100, 90, 127, 110), 0, 0, (10, 60, 30, 115),
   (("IN_CUT", 90), ("LRATE", 14), ("LD_SHP", 30)), IND, "nappe métallique qui sonne comme une tôle tenue",
   "accords A2-A4", "S83")
IN("PC MTL TICK", (127, 100, 0, 40, 30, 60, 30, 0), (0, 36, 0, 32), 0, 0, (20, 0, 20, 40), (("IN_CUT", 127),),
   IND, "tick métallique de percussion indus", "C4-C6", "S117")
IN("PC TOM INDUS", (64, 0, 0, 20, 60, 50, 60, 40), (0, 66, 0, 60), 0, 0, (40, 0, 10, 60), (("IN_CUT", 80),), IND,
   "tom industriel saturé", "C2-C3", "S117")
IN("DR IND RING", (37, 30, 110, 10, 40, 0, 60, 70), (110, 90, 127, 110), 0, 0, (20, 40, 30, 115),
   (("IN_CUT", 70), ("LRATE", 8), ("LD_SHP", 40)), IND, "drone en anneau qui tourne lentement", "A1-A3, tenu",
   "S82")

# ================================================================================================================== FM6
# E: ALGO (0 the patch's) FEEDBACK MOD LVL RATIO MOD ENV VEL MOD DETUNE PATCH (0 EP 1 BELL 2 BASS 3 BRASS 4 PAD
#    5 MARIMBA 6 ORGAN 7 PLUCK)
FM("ST FM ORGAN", (0, 0, 10, 0, 0, 0, 20, 6), (0, 0, 127, 0), 0, 0, (0, 30, 40, 50), (), HG,
   "stab d'orgue house façon M1 (patch ORGAN, tirettes)", "accords A3-A5", "S23", replace="ST FM METAL")
FM("PL FM PLUCK", (0, 0, 10, 0, -10, 2, 10, 7), (0, 0, 127, 0), 0, 0, (0, 30, 60, 50), (), MEL,
   "pluck FM de techno mélodique (patch PLUCK) : attaque riche, queue pure", "A3-A5", "S01")
FM("PL FM GLASS", (0, 0, 0, 5, -20, 0, 10, 1), (0, 0, 127, 0), 0, 0, (0, 20, 70, 80), (), MEL,
   "pluck de verre : la cloche FM au rapport décalé, plus courte", "A4-A6", "S09")
FM("PL FM MALLET", (0, 0, 0, 0, 0, 2, 0, 5), (0, 0, 127, 0), 0, 0, (0, 0, 20, 30), (), MIN,
   "mallet FM (patch MARIMBA) : bois accordé", "A3-A5", "S08")
FM("PL FM KALMBA", (0, 2, 20, 3, -20, 0, 0, 5), (0, 0, 127, 0), 0, 0, (0, 10, 60, 50), (), DUB,
   "kalimba FM : marimba plus métallique", "A3-A5", "S08")
FM("PL FM DUB", (0, 0, -10, 0, -30, 0, 20, 0), (0, 0, 127, 0), 0, 0, (0, 30, 110, 100), (), DUB,
   "ping de dub techno en FM (patch EP), delay et reverb immenses", "A3-A5, un coup", "S06")
FM("PL FM BELL", (0, 3, 30, 7, -40, 0, 0, 1), (0, 0, 127, 0), 0, 0, (0, 0, 40, 60), (), IND,
   "cloche FM courte, très inharmonique", "A4-A6", "S09")
FM("ST FM BRASS", (0, 2, 10, 0, 10, 0, 30, 3), (0, 0, 127, 0), 0, 0, (10, 40, 40, 40), (), DET,
   "stab brass FM (patch BRASS)", "accords A3-A5", "S25")
FM("ST FM PIANO", (0, 0, 20, 0, 0, 3, 20, 0), (0, 0, 127, 0), 0, 0, (10, 30, 30, 50), (), RAV,
   "accords de piano rave (patch EP, plus brillant)", "accords A3-A5", "S30")
FM("BS FM METAL", (0, 3, 20, 7, -10, 0, 0, 2), (0, 0, 127, 0), 0, 1, (30, 0, 5, 10), (), IND,
   "basse FM métallique : modulateur à 7:1, claquement de corde", "A1-A2", "S46")
FM("BS FM KNOCK", (0, 0, 30, -5, -20, 0, 0, 5), (0, 0, 127, 0), 0, 1, (20, 0, 10, 10), (), MIN,
   "basse « knock » : marimba FM grave, courte et boisée", "A1-A2", "S52")
FM("BS FM HARD", (0, 7, 30, 0, 20, 0, 0, 2), (0, 0, 127, 0), 0, 1, (90, 0, 5, 5), (("DTYPE", 1),), HT,
   "basse FM au feedback maximal, saturée", "A1-A2", "S47")
FM("LD FM SCREAM", (0, 7, 40, 2, 20, 0, 20, 3), (0, 0, 127, 0), 0, 1, (80, 0, 30, 30), (("DTYPE", 1),
   ("GLIDE", 16)), IND, "lead FM hurlant (brass au feedback max, drive)", "A3-A5", "S60")
FM("PA FM GLASS", (0, 0, -10, 3, 30, 0, 60, 4), (0, 0, 127, 0), 0, 0, (0, 70, 30, 110), (), MEL,
   "nappe FM vitreuse (patch PAD, modulateurs décalés)", "accords A3-A5", "S85")
FM("PC FM WOOD", (0, 0, 40, 9, -50, 0, 0, 5), (0, 0, 127, 0), 0, 0, (0, 0, 20, 40), (), HG,
   "percussion de bois FM : bloc, très court", "C4-C6", "S119")
FM("PC FM COWBL", (0, 4, 30, 5, -60, 0, 0, 7), (0, 0, 127, 0), 0, 0, (10, 0, 20, 30), (), DET,
   "cloche de vache FM", "C4-C5", "S119")
FM("DR FM BELL", (0, 2, 20, 4, 60, 0, 90, 1), (0, 0, 127, 0), 0, 0, (10, 70, 30, 120),
   (("LRATE", 10), ("LD_PIT", 3)), IND, "drone de cloches : la cloche FM qui ne s'éteint presque plus",
   "A2-A4, tenu", "S83")

# ================================================================================================================ PHASE
# E: WAVE (0 SAW 1 SQR 2 PLS 3 DSIN 4 SPLS 5 RSAW 6 RTRI 7 RTRP) WAVE2 (0 - 1 SAW ..) DCW ENV DETUNE LINE (0 MIX 1 RING)
#    SUB FOLD
PH("PL PD RESO", (5, 0, 70, 70, 0, 0, 0, 0), (0, 62, 0, 54), 0, 0, (0, 20, 60, 40), (), TE,
   "pluck résonant de CZ (onde RSAW) : un « filtre » qui claque sans filtre", "A3-A5", "S04")
PH("PL PD BELL", (6, 3, 90, 40, 0, 1, 0, 0), (0, 70, 0, 64), 0, 0, (0, 20, 50, 60), (), MEL,
   "pluck cloche CZ (RTRI en anneau)", "A4-A6", "S09")
PH("ST PD STAB", (0, 1, 60, 80, 10, 0, 40, 0), (0, 60, 0, 46), 0, 0, (10, 40, 50, 50), (), DET,
   "stab CZ : scie dont la brillance tombe vite, sub", "accords A3-A5", "S20")
PH("ST PD HARD", (2, 0, 90, 50, 0, 0, 20, 80), (0, 58, 0, 40), 0, 0, (60, 0, 40, 40), (("DTYPE", 1),), HT,
   "stab CZ replié et saturé", "accords A2-A4", "S28")
PH("ST PD ORGAN", (3, 0, 40, 10, 0, 0, 60, 0), (0, 62, 60, 40), 0, 0, (0, 70, 30, 50), (), HG,
   "orgue CZ (double sinus), court", "accords A3-A5", "S23")
PH("BS PD RESO", (5, 0, 60, 90, 0, 0, 80, 0), (0, 58, 40, 20), 0, 1, (20, 0, 10, 5), (), ACI,
   "basse CZ résonante : un acid sans filtre", "A1-A2", "S42")
PH("BS PD SUB", (3, 0, 20, 20, 0, 0, 127, 0), (0, 64, 110, 20), 0, 1, (0, 0, 0, 0), (), TE,
   "sub CZ : double sinus + octave dessous", "E1-A1", "S45")
PH("LD PD RING", (0, 1, 80, 30, 20, 1, 0, 20), (0, 70, 110, 40), 0, 1, (30, 0, 40, 30), (("GLIDE", 16),), IND,
   "lead CZ en anneau : deux lignes désaccordées multipliées", "A3-A5", "S60")
PH("LD PD SQUARE", (1, 2, 100, 40, 12, 0, 30, 0), (0, 70, 110, 40), 0, 1, (20, 20, 50, 30),
   (("LRATE", 86), ("LD_PIT", 8), ("LFADE", 70)), TE, "lead carré CZ, vibrato qui arrive", "A3-A5", "S67")
PH("PA PD CHOIR", (3, 6, 50, 20, 14, 0, 0, 0), (90, 90, 120, 100), 0, 0, (0, 90, 30, 110),
   (("LRATE", 20), ("SPRD", 110)), MEL, "chœur synthétique CZ", "accords A3-A5", "S87")
PH("PA PD DARK", (7, 0, 30, 40, 0, 0, 40, 0), (100, 90, 120, 110), 0, 0, (0, 90, 20, 120),
   (('LRATE', 10), ('LD_SHP', 20)), IND, "pad CZ sombre (RTRP), sub", "accords A2-A4", "S81")
PH("DR PD FOLD", (0, 0, 60, 20, 0, 0, 60, 70), (110, 90, 127, 110), 0, 0, (20, 40, 20, 115),
   (("LRATE", 6), ("LD_SHP", 30)), IND, "drone CZ replié qui tourne", "A1-A3, tenu", "S82")

# ================================================================================================================= TRIO
# E: WAVE (0 SAW3 1 PLS3 2 PPT 3 SST 4 TRI3 5 S&T 6 P&S 7 P+N 8 NOIS 9 SYNC 10 SYNCP 11 SYNC3 12 RING 13 RING3
#    14 R+S 15 R+SP) PITCH2 PITCH3 DETUNE MODE (0 LP 1 BP 2 HP 3 NOT) CUTOFF RES PW
TR("ST TRIO MIN", (0, 3, 7, 10, 0, 96, 20, 64), (0, 60, 10, 46), 30, 0, (10, 40, 40, 40), (), DET,
   "stab mineur d'une touche : trois scies en triade mineure (le stab de « The Bells »)", "A2-A4, une note",
   "S27")
TR("ST TRIO MIN7", (1, 3, 10, 6, 0, 40, 60, 40), (0, 70, 0, 60), 30, 0, (0, 40, 90, 80), (), DUB,
   "accord mineur 7 d'une touche (pulses), pour le dub", "A2-A4, une note", "S21")
TR("ST TRIO 5TH", (0, 7, 12, 8, 0, 60, 40, 64), (0, 62, 30, 44), 30, 0, (60, 20, 50, 40), (('DTYPE', 1),), RAV,
   "stab de quinte + octave d'une touche", "A2-A4, une note", "S22")
TR("ST TRIO SUS", (4, 5, 7, 4, 1, 70, 50, 64), (0, 60, 0, 50), 20, 0, (0, 40, 60, 60), (), TE,
   "stab suspendu (fondamentale, quarte, quinte) en triangles, filtre passe-bande", "A3-A5, une note", "S27")
TR("PC COWBELL", (1, 8, -24, 0, 1, 90, 30, 64), (0, 46, 0, 40), 0, 0, (0, 0, 10, 20), (), DET,
   "cowbell 808 : deux carrés à une sixte, passe-bande, court", "C4-C5", "S119")
TR("PL CHIP", (1, 19, 0, 0, 0, 120, 10, 30), (0, 44, 0, 36), 0, 0, (0, 0, 50, 30), (), MIN,
   "pluck chiptune : pulses étroites à l'octave et la quinte", "A4-A6", "S12")
TR("PL RING", (12, 0, 19, 0, 0, 90, 20, 64), (0, 54, 0, 46), 0, 0, (0, 20, 60, 50), (), IND,
   "pluck en anneau, cloche de jouet", "A3-A5", "S09")
TR("PL TRIO BP", (0, 12, 0, 10, 1, 80, 70, 64), (0, 56, 0, 46), 40, 0, (0, 30, 60, 40), (), HG,
   "pluck passe-bande nasal qui claque", "A3-A5", "S04")
TR("BS TRIO RING", (13, 0, -12, 6, 0, 60, 40, 64), (0, 60, 60, 20), 30, 1, (30, 0, 10, 5), (), IND,
   "basse en anneau métallique", "A1-A2", "S46", replace="BS TRIO SYNC")
TR("BS TRIO 5TH", (0, 7, 12, 6, 0, 70, 60, 64), (0, 56, 90, 22), 40, 1, (60, 0, 5, 5), (("DTYPE", 1),), HT,
   "basse de quinte saturée (fondamentale, quinte, octave au-dessus)", "A1-A2", "S47")
TR("BS TRIO PLS", (1, 0, -12, 4, 1, 60, 90, 20), (0, 58, 90, 20), 40, 1, (10, 0, 10, 5), (), MIN,
   "basse pulse étroite, nasale, courte", "A1-A2", "S44")
TR("LD TRIO PWM", (1, 0, 0, 20, 0, 80, 50, 64), (2, 72, 112, 55), 8, 1, (0, 20, 40, 50),
   (('GLIDE', 20), ('LRATE', 40), ('LD_SHP', 40)), RAV, "lead de pulses désaccordées, PWM qui tourne",
   "accords A3-A5", "S67", replace="LD TRIO SAW")
TR("LD TRIO RING", (14, 12, 7, 0, 0, 90, 40, 64), (0, 70, 110, 40), 10, 1, (20, 0, 40, 30), (("GLIDE", 16),),
   IND, "lead anneau + sync, métallique et criard", "A3-A5", "S60")
TR("LD CHIP", (10, 12, 0, 0, 0, 110, 10, 30), (0, 70, 110, 40), 0, 1, (0, 0, 90, 30),
   (('LRATE', 96), ('LD_PIT', 24), ('LFADE', 60)), MIN, "lead chiptune sync, vibrato", "A4-A6", "S69")
TR("PA TRIO NTCH", (0, 0, 12, 20, 3, 60, 80, 64), (90, 90, 120, 100), 0, 0, (0, 90, 30, 110),
   (("LRATE", 10), ("LD_FLT", 30), ("SPRD", 110)), TE, "pad filtré au coupe-bande qui balaie (effet phaser)",
   "accords A2-A4", "S80")
TR("DR TRIO RING", (13, 0, 31 - 24, 20, 1, 50, 80, 64), (110, 90, 127, 110), 0, 0, (20, 40, 30, 115),
   (("LRATE", 6), ("LD_FLT", 30)), IND, "drone en anneau, passe-bande résonant qui erre", "A1-A3, tenu", "S82")
TR("FX TRIO ZAP", (9, 24, 0, 0, 0, 110, 40, 64), (0, 64, 0, 56), 40, 0, (20, 0, 80, 50), (("ED_PIT", 63),), DET,
   "zap sync : la hauteur et la sync tombent en un éclair", "A3-A5", "S96")
TR("PC NOISE SNR", (8, 0, 0, 0, 1, 90, 30, 64), (0, 52, 0, 46), 20, 0, (20, 0, 10, 40), (), HG,
   "snare de bruit chip, passe-bande", "C3-C5", "S120")

# ================================================================================================================ GRAIN
# E: SOURCE (0 PIANO 2 FLUTE 3 SAX) POS SIZE DENSITY PITCH SPREAD RANDOM TONE
GR("PA GRAIN AIR", (2, 30, 110, 70, 12, 60, 20, 110), (100, 90, 120, 110), 0, 0, (0, 60, 30, 115), (), MEL,
   "nappe de souffle : grains de flûte une octave au-dessus", "accords A3-A5", "S85")
GR("PA GRN DARK", (2, 40, 120, 70, -12, 30, 10, 70), (100, 90, 127, 110), 0, 0, (10, 40, 20, 120), (), IND,
   "nappe sombre : grains de sax une octave dessous, filtrés", "accords A2-A4", "S81")
GR("PL GRAIN", (0, 10, 20, 60, 0, 10, 10, 100), (0, 60, 0, 50), 0, 0, (0, 20, 60, 50), (), MIN,
   "pluck granulaire : petits grains de piano, une attaque qui s'émiette", "A3-A5", "S11")
GR("FX STUTTER", (0, 64, 10, 127, 0, 0, 0, 127), (0, 70, 100, 30), 0, 0, (10, 0, 40, 30), (), MIN,
   "bégaiement : grains très courts et réguliers, effet de glitch", "A3-A5", "S99")
GR("DR GRAIN SAX", (3, 80, 127, 40, -24, 80, 40, 40), (110, 90, 127, 110), 0, 0, (20, 40, 20, 120), (), IND,
   "drone granulaire de sax, deux octaves dessous", "A2-A3, tenu", "S82")
GR("ST GRAIN", (0, 0, 40, 90, 0, 20, 0, 110), (0, 58, 0, 46), 0, 0, (0, 30, 50, 60), (), RAV,
   "stab de piano granulaire (accords rave)", "accords A3-A5", "S30")
GR("LD GRN FLUTE", (2, 50, 60, 110, 0, 10, 30, 110), (10, 70, 120, 50), 0, 1, (0, 40, 50, 60),
   (("LRATE", 80), ("LD_PIT", 8)), DUB, "lead de flûte granulaire, soufflé, vibrato", "A4-A5", "S68")

# ================================================================================================================= PHYS
# E: MODEL (0 MODAL 1 STRNG 2 MEMB 3 SYMP) STRUCT BRIGHT DAMP POS ACCENT BOW EXCITE
PY("PL STRING", (1, 70, 100, 70, 20, 100, 0, 0), (0, 100, 127, 50), 0, 0, (0, 20, 50, 40), (), MEL,
   "corde pincée brillante (Karplus-Strong)", "A3-A5", "S10")
PY("PL MUTED", (1, 60, 50, 30, 50, 127, 0, 20), (0, 100, 127, 30), 0, 0, (0, 10, 60, 30), (), MIN,
   "corde étouffée, très courte (palm mute)", "A2-A4", "S10")
PY("PL SITAR", (3, 0, 90, 80, 30, 100, 40, 0), (0, 100, 127, 60), 0, 0, (0, 20, 50, 60), (), DUB,
   "pluck de sitar : chevalet bourdonnant, cordes sympathiques", "A3-A5", "S10")
PY("PL MARIMBA", (0, 70, 60, 50, 30, 100, 0, 30), (0, 100, 127, 40), 0, 0, (0, 0, 50, 40), (), MIN,
   "marimba modale : lames accordées", "A3-A5", "S08")
PY("PL STEEL", (0, 110, 110, 60, 20, 110, 0, 20), (0, 100, 127, 50), 0, 0, (0, 20, 60, 60), (), IND,
   "pluck d'acier : partiels inharmoniques, court", "A3-A5", "S09")
PY("BS BONGO", (2, 100, 50, 60, 40, 100, 70, 20), (0, 100, 127, 50), 0, 1, (20, 0, 10, 20), (), HG,
   "basse de bongo accordé : la peau, sa hauteur qui tombe (bongo pitché à la Setaoc Mass)", "A1-A2", "S52")
PY("BS STRING", (1, 40, 60, 100, 30, 127, 0, 0), (0, 100, 127, 40), 0, 1, (10, 0, 10, 10), (), DUB,
   "basse de corde pincée, ronde", "A1-A2", "S44")
PY("PC CONGA", (2, 127, 110, 40, 100, 110, 0, 60), (0, 100, 127, 40), 0, 0, (0, 0, 10, 30), (), HG,
   "conga : peau chargée, frappe vers le bord", "C3-C4", "S116")
PY("PC RIM", (0, 90, 127, 30, 10, 127, 0, 90), (0, 100, 127, 20), 0, 0, (0, 0, 20, 30), (), MIN,
   "rim / bloc : modes brillants très courts, beaucoup de frappe", "C4-C5", "S119")
PY("PC GONG", (0, 100, 70, 110, 20, 127, 0, 10), (0, 100, 127, 100), 0, 0, (10, 30, 20, 100), (), IND,
   "gong grave qui chante", "C2-C3", "S117")
PY("PC TIMPANI", (2, 0, 60, 80, 20, 110, 30, 10), (0, 100, 127, 70), 0, 0, (0, 0, 10, 60), (), IND,
   "timbale : peau idéale, grave et longue", "C2-C3", "S116")
PY("LD BOW", (1, 60, 80, 110, 30, 127, 127, 0), (20, 100, 127, 60), 0, 1, (0, 30, 40, 60), (("GLIDE", 20),), DUB,
   "lead de corde frottée", "A3-A5", "S68")
PY("PA BOWED", (0, 60, 60, 120, 20, 100, 127, 0), (90, 90, 127, 100), 0, 0, (0, 50, 30, 110), (), MEL,
   "nappe frottée : lames tenues par l'archet", "accords A3-A5", "S85")
PY("DR METAL BOW", (0, 120, 90, 127, 30, 127, 127, 60), (90, 90, 127, 110), 0, 0, (20, 40, 20, 120), (), IND,
   "texture : tôle frottée, partiels très inharmoniques", "A2-A4, tenu", "S83")
PY("FX SPRING", (1, 0, 127, 100, 0, 127, 0, 100), (0, 100, 127, 80), 0, 0, (30, 0, 30, 90), (("ED_PIT", -40),),
   IND, "ressort : corde tendue, chevalet courbe, choc", "A1-A3", "S98")

# ================================================================================================================ NOISE
# E: MODE (0 ANLG 1 DUST 2 LFSR 3 META) COLOR CUTOFF RES KEYTRK DENSITY DRIFT CRUSH
NZ("FX SWEEP UP", (0, 30, 20, 100, 0, 0, 0, 0), (60, 90, 127, 60), 0, 0, (0, 50, 30, 90),
   (("LRATE", 2), ("LD_FLT", 63), ("LWAVE", 2)), HTR, "sweep de bruit : passe-bande résonant qui monte",
   "A3, tenu 4 mesures", "S92")
NZ("FX NOISE HIT", (0, 0, 110, 0, 0, 0, 0, 0), (0, 80, 0, 90), 0, 0, (40, 0, 30, 120), (), TE,
   "impact de bruit blanc, reverb énorme", "A3, un coup", "S94")
NZ("FX VINYL", (1, 20, 100, 20, 64, 70, 0, 40), (0, 90, 127, 40), 0, 0, (0, 0, 0, 10), (), DUB,
   "craquements de vinyle (DUST, écrasé)", "A3, tenu", "S100")
NZ("FX STATIC", (2, 0, 100, 0, 127, 40, 60, 90), (0, 90, 127, 40), 0, 0, (20, 0, 10, 40), (), IND,
   "statique de radio : registre à décalage, grésillement", "A3, tenu", "S100")
NZ("FX WASH", (0, 100, 50, 30, 0, 0, 60, 0), (90, 100, 0, 110), 0, 0, (0, 60, 40, 127), (), HTR,
   "vague de bruit brun qui monte et s'en va", "A3, un coup", "S92")
NZ("PL LFSR BLIP", (2, 0, 127, 0, 127, 100, 0, 60), (0, 40, 0, 36), 0, 0, (0, 0, 50, 30), (), MIN,
   "blip numérique (registre court) accordé", "C4-C6", "S12")
NZ("PL META", (3, 0, 110, 50, 127, 50, 0, 0), (0, 56, 0, 50), 20, 0, (0, 20, 60, 50), (), IND,
   "pluck métallique (META) : un timbre de registre, accordé", "A3-A5", "S09")
NZ("PC HAT NOISE", (0, 0, 127, 10, 0, 0, 0, 0), (0, 38, 0, 34), 0, 0, (0, 0, 20, 20),
   (("ITYPE", 9), ("IA", 110), ("IC", 127)), TE, "charley de bruit : passe-haut, très court", "C4-C6", "S113")
NZ("PC SNARE NZ", (0, 60, 100, 40, 64, 0, 0, 0), (0, 54, 0, 50), 0, 0, (30, 0, 10, 50), (), TE,
   "snare de bruit rose, corps résonant", "C3-C4", "S120")
NZ("PC SHAKER", (1, 0, 127, 0, 0, 127, 0, 0), (0, 52, 0, 40), 0, 0, (0, 0, 10, 20), (), HG,
   "shaker : poussière dense très courte", "C4-C6", "S116")
NZ("PC CLAP NZ", (0, 30, 90, 50, 0, 90, 0, 0), (0, 52, 0, 56), 0, 0, (20, 0, 10, 70), (), TE,
   "clap de bruit : impulsions + bruit, passe-bande", "C3-C4", "S120")
NZ("LD LFSR", (2, 0, 127, 0, 127, 112, 0, 70), (0, 70, 110, 40), 0, 1, (20, 0, 40, 30), (("GLIDE", 14),), MIN,
   "lead numérique 8 bits (registre très court)", "A3-A5", "S69")
NZ("PA NOISE", (0, 80, 60, 120, 127, 0, 50, 0), (100, 90, 127, 110), 0, 0, (0, 60, 30, 120), (), TE,
   "nappe de bruit accordé : passe-bande étroit sur chaque note de l'accord", "accords A3-A5", "S87")
NZ("DR RADIO", (2, 0, 60, 60, 64, 90, 90, 100), (110, 90, 127, 110), 0, 0, (20, 40, 30, 115),
   (("LRATE", 10), ("LD_FLT", 30)), IND, "radio entre deux stations : registre, dérive, filtre qui erre",
   "A2-A4, tenu", "S100")

# ================================================================================================================= DRUM
# E: KIT (0 STD 4 808 5 "10" 6 66 7 55 8 77 9 DEEP 10 TIGHT 11 PERC) PITCH TONE DECAY SNAP ACCENT KICK (0 PUNCH 1 ROUND)
#    DRV
DM("KT BERLIN", (10, 50, 50, 110, 60, 110, 1, 40), (0, 100, 127, 100), 0, 0, (10, 0, 0, 90), (), TE,
   "kit berlinois : kick long et rond, hats sombres, reverb", "beat 4/4, 130-140", "S113")
DM("KT DETROIT", (6, 64, 110, 50, 90, 110, 0, 40), (0, 100, 127, 100), 0, 0, (20, 0, 0, 40), (), DET,
   "kit techno de Detroit (le modèle « 10 »), net et sec", "beat 4/4", "S114")
DM("KT SCHRANZ", (5, 66, 110, 40, 120, 127, 0, 127), (0, 100, 127, 100), 0, 0, (80, 0, 0, 10), (("DTYPE", 1),),
   SCH, "kit schranz : tout saturé, hats tranchants, très court", "beat 150-160", "S111")
DM("KT HARDGROOV", (11, 64, 90, 60, 100, 120, 0, 80), (0, 100, 127, 100), 0, 0, (50, 0, 0, 20), (), HG,
   "kit hardgroove : percussions latines distordues (kit PERC)", "boucles 1-2 mesures", "S112")
DM("KT MINIMAL", (10, 70, 60, 30, 80, 90, 0, 0), (0, 100, 127, 100), 0, 0, (0, 0, 20, 30), (), MIN,
   "kit minimal : tout court, clicks", "beat 4/4 sec", "S118")
DM("KT GABBER", (5, 50, 120, 60, 127, 127, 0, 127), (0, 100, 127, 100), 0, 0, (120, 0, 0, 10),
   (("DTYPE", 4),), HTK, "kit gabber : kick FUZZ énorme", "beat 170-190", "S115")
DM("KT DUB", (9, 56, 40, 64, 30, 90, 0, 0), (0, 100, 127, 100), 0, 0, (0, 0, 40, 80), (), DUB,
   "kit dub techno : profond, sombre, delay et reverb", "beat 4/4 lent", "S113")
DM("KT TRIBE", (6, 60, 90, 70, 90, 120, 0, 100), (0, 100, 127, 100), 0, 0, (70, 0, 10, 30), (("DTYPE", 1),),
   HTK, "kit hardtek / tribe : kick saturé « whack », toms", "beat 150-180", "S110")
DM("KT ACID HSE", (8, 64, 110, 40, 110, 100, 0, 70), (0, 100, 127, 100), 0, 0, (20, 0, 0, 20), (), ACI,
   "kit acid house (modèle 77)", "beat 4/4", "S114")
DM("KT LOFI 55", (7, 60, 50, 70, 50, 100, 1, 60), (0, 100, 127, 100), 0, 0, (30, 0, 10, 30), (("DTYPE", 3),),
   HG, "kit boîte à rythmes 55 sale (CRUSH)", "beat lo-fi", "S112")

# =============================================================================================================== DRUM-X
# E: MORPH PITCH COLOR DECAY NOISE FM FOLD DRIVE; kit = DX_KITS + 1
DX("KT DX BERLIN", (60, 58, 50, 64, 50, 10, 0, 20), (0, 100, 127, 100), 0, 0, (10, 0, 10, 50), (), TE,
   "kit DRUM-X berlinois : sombre, long", "beat 4/4", "S113", kit=1)
DX("KT DX DUB", (50, 54, 40, 64, 40, 0, 0, 0), (0, 100, 127, 100), 0, 0, (0, 0, 40, 90), (), DUB,
   "kit DRUM-X dub : grave, delay et reverb", "beat 4/4 lent", "S113", kit=2)
DX("KT DX MINIML", (70, 70, 80, 30, 70, 20, 0, 0), (0, 100, 127, 100), 0, 0, (0, 0, 30, 20), (), MIN,
   "kit DRUM-X minimal : clicks très courts", "beat sec", "S118", kit=3)
DX("KT DX METAL", (90, 60, 100, 60, 90, 90, 110, 110), (0, 100, 127, 100), 0, 0, (60, 0, 10, 40), (), IND,
   "kit DRUM-X industriel replié et saturé", "beat 140-150", "S117", kit=1)
DX("KT DX TRIBAL", (64, 60, 70, 64, 50, 20, 20, 60), (0, 100, 127, 100), 0, 0, (30, 0, 10, 40), (), HG,
   "kit DRUM-X tribal : toms et peaux saturés", "boucles", "S116", kit=7)
DX("KT DX GLITCH", (80, 70, 90, 40, 80, 80, 40, 40), (0, 100, 127, 100), 0, 0, (20, 0, 50, 30), (), MIN,
   "kit DRUM-X glitch : FM, bribes, delay", "beat cassé", "S99", kit=6)
DX("KT DX HARD", (70, 60, 100, 50, 90, 30, 40, 127), (0, 100, 127, 100), 0, 0, (90, 0, 0, 15),
   (("DTYPE", 1),), HT, "kit DRUM-X hard techno : drive maximal", "beat 145-155", "S110", kit=1)


# ======================================================================================================== second half
# ---------------------------------------------------------------------------------------------------- SUPERSAW
SS("LD SS DETUNE", (127, 110, 127, 10, 100, 0, 64, 0), (0, 70, 115, 50), 0, 0, (0, 0, 40, 40), (), HTR,
   "supersaw au désaccord maximal, sec (sans chorus ni spread)", "A3-A5", "S62")
SS("LD SS FILTER", (70, 100, 60, 80, 64, 20, 64, 0), (0, 70, 115, 50), 0, 0, (10, 40, 50, 50),
   (("SPRD", 100), ("LRATE", 80), ("LSYNC", 7), ("LD_FLT", 30)), HTR,
   "supersaw au filtre résonant qui bat en 1/8 triolet (LFO calé sur le tempo)", "A3-A5", "S62")
SS("ST SS SCHRNZ", (80, 100, 110, 20, 90, 120, 64, 30), (0, 50, 0, 30), 10, 0, (120, 0, 20, 20),
   (("DTYPE", 4),), SCH, "stab schranz : supersaw dans la FUZZ, très court", "A2-A4", "S28")
SS("BS SS REESE", (110, 120, 20, 90, 30, 100, 64, 80), (0, 64, 120, 30), 6, 1, (40, 30, 0, 10), (), HT,
   "reese en supersaw : sept scies très désaccordées, fermé, saturé, sub", "A1-A2", "S41")
SS("FX SS RISER", (127, 127, 30, 50, 64, 30, 64, 0), (120, 90, 127, 90), 63, 0, (20, 90, 40, 100),
   (("SPRD", 127), ("ED_PIT", 63), ("ED_SHP", 63)), HTR,
   "riser supersaw : filtre, désaccord et hauteur montent avec l'attaque", "A2-A3, tenu 8 mesures", "S90")
SS("PL SS SQUISH", (50, 80, 30, 110, 64, 20, 64, 0), (0, 58, 0, 48), 50, 0, (10, 40, 60, 40), (("SPRD", 90),), TE,
   "pluck supersaw résonant qui « squishe »", "A3-A5", "S07")
# ------------------------------------------------------------------------------------------------------ HOOVER
HV("LD HOOV SLOW", (90, 110, 90, 40, 104, 15, 40, 10), (10, 70, 115, 60), 0, 1, (10, 110, 40, 60),
   (("SPRD", 110), ("GLIDE", 30)), RAV, "hoover à la plongée lente (~3 s) : la note met longtemps à arriver",
   "A2-A4, notes longues", "S63")
HV("PA HOOV DARK", (0, 0, 100, 60, 50, 30, 60, 30), (100, 90, 120, 110), 0, 0, (10, 90, 30, 115),
   (("SPRD", 120), ("LRATE", 10), ("LD_FLT", 20)), IND, "nappe hoover sombre, filtrée, sub", "accords A2-A4", "S81")
HV("DR HOOVER", (127, 127, 127, 80, 80, 40, 80, 60), (110, 90, 127, 110), 0, 0, (30, 100, 20, 120),
   (("LRATE", 6), ("LD_FLT", 20)), IND, "drone hoover qui plonge interminablement", "A1-A3, tenu", "S82")
# -------------------------------------------------------------------------------------------------------- ACID
AC("BS 303 FUNK", (1, 60, 40, 40, 100, 60, 0, 127), (0, 100, 127, 14), 0, 1, (0, 50, 30, 10), (), ACI,
   "303 funky : carré, slides très longs, peu de résonance", "A1-A2, beaucoup de slides", "S42")
AC("BS 303 MUTED", (2, 10, 40, 40, 40, 40, 0, 45), (0, 100, 127, 12), 0, 1, (0, 0, 10, 10), (), DUB,
   "303 étouffé : scie + sub, filtre presque fermé, rond", "A1-A2", "S42")
# -------------------------------------------------------------------------------------------------------- GRAIN
GR("PA GRN SHIMR", (2, 60, 90, 120, 24, 80, 30, 127), (100, 90, 120, 110), 0, 0, (0, 80, 60, 127), (), MEL,
   "nappe scintillante : grains de flûte deux octaves au-dessus", "accords A3-A5", "S85")
GR("DR GRN CLOUD", (0, 64, 110, 70, 0, 127, 60, 60), (110, 90, 127, 110), 0, 0, (10, 50, 30, 127), (), IND,
   "nuage de piano éparpillé, très dense", "A2-A4, tenu", "S83")
GR("FX GRN SCATR", (3, 64, 6, 60, 0, 127, 127, 120), (0, 80, 60, 70), 0, 0, (0, 20, 70, 60), (), MIN,
   "éclats de sax minuscules, au hasard", "A3-A5", "S99")
GR("PL GRN FLUTE", (2, 20, 30, 80, 0, 10, 10, 110), (0, 62, 0, 52), 0, 0, (0, 30, 60, 60), (), DUB,
   "pluck de flûte granulaire, soufflé", "A4-A6", "S06")
GR("ST GRN SAX", (3, 30, 50, 100, 0, 30, 10, 100), (0, 62, 0, 48), 0, 0, (20, 30, 50, 50), (), HG,
   "stab de sax granulaire, rugueux", "accords A3-A4", "S28")
GR("LD GRN SAX", (3, 40, 80, 80, 0, 20, 20, 90), (10, 70, 120, 50), 0, 1, (30, 20, 50, 50), (("GLIDE", 20),),
   DUB, "lead de sax granulaire, glide", "A3-A5", "S68")
GR("FX GRN FREEZ", (0, 90, 110, 70, 12, 0, 0, 80), (60, 90, 127, 100), 0, 0, (0, 60, 40, 120), (), MEL,
   "piano figé : grains longs, position fixe, une octave au-dessus", "A3-A5, tenu", "S85")
# ------------------------------------------------------------------------------------------------------- DRUM-X
DX("KT DX SCHRNZ", (80, 66, 110, 40, 110, 50, 60, 127), (0, 100, 127, 100), 0, 0, (110, 0, 0, 10),
   (("DTYPE", 4),), SCH, "kit DRUM-X schranz : FUZZ, hats tranchants, très court", "beat 150-160", "S111", kit=4)
DX("KT DX ACID", (60, 64, 80, 50, 70, 30, 0, 30), (0, 100, 127, 100), 0, 0, (20, 0, 20, 20), (), ACI,
   "kit DRUM-X acid house : sec, claquant", "beat 4/4", "S114", kit=3)
DX("KT DX TRIBE", (70, 54, 100, 64, 60, 40, 60, 110), (0, 100, 127, 100), 0, 0, (90, 0, 10, 30),
   (("DTYPE", 1),), HTK, "kit DRUM-X hardtek / tribe : toms tribaux saturés", "beat 150-180", "S110", kit=7)
DX("KT DX DETROI", (50, 68, 70, 60, 70, 10, 0, 20), (0, 100, 127, 100), 0, 0, (0, 0, 20, 40), (), DET,
   "kit DRUM-X de Detroit : clair, un peu d'espace", "beat 4/4", "S114", kit=5)
DX("KT DX GABBER", (90, 50, 127, 50, 100, 80, 100, 127), (0, 100, 127, 100), 0, 0, (127, 0, 0, 10),
   (("DTYPE", 2),), HTK, "kit DRUM-X gabber : kick replié au maximum", "beat 170-190", "S115", kit=4)
DX("KT DX ELECTR", (30, 46, 40, 64, 30, 90, 30, 50), (0, 100, 127, 100), 0, 0, (20, 0, 60, 40), (), DET,
   "kit DRUM-X électro laser : longues queues, FM", "beat électro", "S121", kit=8)
# --------------------------------------------------------------------------------------------------------- DRUM
DM("KT TIGHT DRY", (10, 72, 90, 20, 110, 100, 0, 0), (0, 100, 127, 100), 0, 0, (0, 0, 0, 0), (), MIN,
   "kit très court et sec, sans reverb", "beat 4/4", "S118")
DM("KT INDUS HRD", (9, 48, 110, 68, 100, 127, 0, 120), (0, 100, 127, 100), 0, 0, (100, 0, 0, 60),
   (("DTYPE", 2),), IND, "kit industriel : DEEP replié, reverb", "beat 135-145", "S117")
DM("KT RAVE 90S", (5, 70, 100, 56, 90, 110, 0, 50), (0, 100, 127, 100), 0, 0, (40, 0, 20, 40), (), RAV,
   "kit rave des années 90 (modèle « 10 »), poussé", "beat 4/4", "S114")
# ------------------------------------------------------------------------------------------------------- ANALOG
AN("LD RES WHSTL", (2, 0, 0, 0, 90, 127, 0, 127), (10, 70, 120, 50), 0, 1, (0, 0, 60, 60),
   (("LRATE", 90), ("LD_PIT", 6), ("GLIDE", 20)), MIN, "sifflement de filtre en auto-oscillation (suit le clavier)",
   "A4-A5", "S68")
AN("BS ENV2 WOB", (1, 8, 64, 0, 40, 70, 20, 64), (0, 64, 110, 30), 0, 1, (20, 0, 10, 10),
   (("M1SRC", 9), ("M1DST", 2), ("M1AMT", 60), ("EG2_DEC", 80)), TE,
   "basse dont ENV 2 ouvre le filtre plus lentement que l'ampli (« wow »)", "A1-A2", "S51")
AN("BS GLIDE SQR", (1, 0, 0, 0, 80, 90, 20, 64), (0, 64, 100, 24), 30, 1, (40, 0, 20, 10), (("GLIDE", 50),), ACI,
   "basse carrée au glide long (notes liées)", "A1-A2", "S42")
AN("BS RUMBLE 2", (3, 10, 64, 0, 40, 0, 90, 0), (0, 90, 40, 80), 0, 1, (80, 0, 50, 120), (("DTONE", -50),), HT,
   "rumble plus sombre et plus long : sinus désaccordés saturés, reverb énorme", "A1, F1", "S40")
AN("ST NOISE STB", (0, 10, 64, 90, 70, 40, 30, 64), (0, 54, 0, 40), 40, 0, (30, 0, 50, 60), (), IND,
   "stab scie + bruit, court et sale", "accords A2-A4", "S28")
AN("ST GATE TRNC", (0, 30, 64, 0, 90, 20, 10, 64), (0, 64, 127, 40), 10, 0, (0, 60, 40, 60),
   (('SPRD', 100), ('LRATE', 80), ('LSYNC', 10), ('LD_AMP', 127)), HTR,
   "accord haché en 1/32 (bégaiement trance)", "accords A3-A5, tenus", "S86")
AN("PA PWM SLOW", (4, 6, 64, 0, 60, 20, 0, 64), (110, 90, 120, 110), 0, 0, (0, 110, 30, 110),
   (("LRATE", 14), ("LD_SHP", 40), ("SPRD", 120)), DET, "pad PWM très lent, large", "accords A2-A4", "S88")
AN("PA NOISE SAW", (0, 20, 64, 60, 50, 30, 0, 64), (90, 90, 120, 100), 0, 0, (0, 70, 30, 115),
   (("LRATE", 20), ("LD_FLT", 20)), IND, "pad de scies + souffle, filtre qui respire", "accords A2-A4", "S81")
AN("FX WHOOSH", (0, 0, 0, 127, 30, 60, 0, 0), (60, 90, 0, 90), 63, 0, (0, 60, 30, 110), (), TE,
   "whoosh : bruit filtré qui s'ouvre et se referme", "A3, un coup", "S92")
AN("FX SIREN UP", (3, 0, 0, 0, 127, 0, 20, 64), (90, 90, 127, 50), 0, 0, (20, 0, 40, 60),
   (("LRATE", 30), ("LWAVE", 2), ("LD_PIT", 63)), RAV, "sirène qui monte d'une octave en dents de scie",
   "A3-A4, tenu", "S97")
AN("PC KICK ANA", (3, 0, 0, 0, 127, 0, 40, 0), (0, 70, 0, 60), 0, 0, (20, 0, 0, 10), (("ED_PIT", 63),), TE,
   "kick analogique : sinus qui tombe d'une octave, drive", "C1-C2", "S110")
AN("PC ZAP PERC", (2, 0, 0, 40, 127, 0, 0, 127), (0, 46, 0, 40), 0, 0, (0, 0, 40, 30), (("ED_PIT", 63),), DET,
   "percussion zap (triangle + bruit qui tombe)", "C3-C5", "S96")
# --------------------------------------------------------------------------------------------------------- MINI
MI("ST STACCATO", (64, 40, 100, 0, 50, 0, 0, 0), (0, 52, 0, 36), 0, 0, (10, 30, 50, 40), (), RAV,
   "stab staccato très sec (patch STACCATO)", "accords A3-A5", "S22", slot=107)
MI("ST PIPE ORG", (100, 0, 0, 0, 60, 127, 0, 0), (10, 60, 110, 50), 0, 0, (0, 40, 30, 90), (), IND,
   "orgue à tuyaux sombre (patch PIPE ORGAN)", "accords A2-A4", "S23", slot=71)
MI("BS MINI BASS", (20, 100, 100, 0, 40, 0, 0, 0), (0, 58, 0, 24), 0, 1, (20, 0, 10, 5), (), TE,
   "basse Minimoog classique (patch MINI BASS), emphasis haute", "A1-A2", "S50", slot=30, bass=1)
MI("BS OCT PUMP", (20, 80, 90, 0, 40, 0, 0, 0), (0, 58, 0, 22), 0, 1, (0, 0, 10, 5), (), HTR,
   "basse octave pompante (patch OCTAVE PUMP)", "A1-A2, 1/8", "S44", slot=34, bass=1)
MI("PA MG SWEEP", (30, 60, 80, 110, 100, 60, 0, 0), (100, 90, 120, 110), 0, 0, (0, 80, 30, 110), (), HTR,
   "nappe qui s'ouvre lentement (patch SWEEP PAD)", "accords A2-A4", "S80", slot=44)
MI("PA MG PULSE", (60, 30, 30, 0, 80, 100, 20, 0), (90, 90, 120, 100), 0, 0, (0, 100, 30, 100), (), TE,
   "pad de pulses (patch PULSE PAD)", "accords A2-A4", "S88", slot=47)
MI("DR SPACE", (50, 40, 50, 60, 90, 80, 0, 0), (110, 90, 127, 120), 0, 0, (0, 60, 40, 127), (), MEL,
   "drone spatial (patch SPACE DRONE)", "A2-A4, tenu", "S82", slot=5)
MI("FX ROBOT MG", (70, 60, 60, 0, 70, 80, 0, 0), (0, 80, 100, 60), 0, 0, (20, 0, 40, 50), (), IND,
   "robot (patch ROBOT)", "A3-A4", "S98", slot=98)
MI("FX THUNDER", (30, 30, 60, 0, 110, 60, 0, 0), (0, 110, 40, 110), 0, 0, (40, 0, 20, 120), (), IND,
   "tonnerre (patch THUNDER)", "A1-A2, un coup", "S94", slot=91)
MI("PL STARDUST", (100, 20, 20, 0, 60, 0, 0, 0), (0, 64, 0, 60), 0, 0, (0, 60, 70, 90), (), MEL,
   "poussière d'étoile (patch STAR DUST), aigu et cristallin", "A4-A6", "S09", slot=81)
MI("PL MUSIC BOX", (70, 0, 40, 0, 60, 0, 0, 0), (0, 70, 0, 64), 0, 0, (0, 20, 50, 70), (), MIN,
   "boîte à musique (patch MUSIC BOX)", "A4-A6", "S09", slot=77)
MI("PL MG HARP", (90, 20, 20, 0, 70, 0, 0, 0), (0, 74, 0, 70), 0, 0, (0, 40, 40, 80), (), MEL,
   "harpe synthétique (patch HARP)", "A3-A5", "S10", slot=69)
# -------------------------------------------------------------------------------------------------------- INDUS
IN("FX DRILL", (110, 80, 60, 60, 60, 0, 60, 30), (0, 70, 127, 40), 0, 0, (40, 0, 20, 60),
   (("IN_CUT", 110), ("LRATE", 110), ("LD_PIT", 20)), IND, "perceuse : FM + bruit, hauteur qui vibre très vite",
   "A3-A4, tenu", "S98")
IN("FX GLITCH IN", (50, 127, 80, 0, 80, 0, 30, 0), (0, 64, 0, 50), 0, 0, (30, 0, 60, 30),
   (('IN_CUT', 127), ('LRATE', 110), ('LWAVE', 4), ('LD_PIT', 40)), IND,
   "glitch : FM dont la hauteur saute au hasard (S&H)", "A3-A5", "S99")
IN("FX ALARM IN", (64, 0, 60, 0, 30, 0, 60, 80), (0, 70, 127, 40), 0, 0, (30, 0, 30, 50),
   (('IN_CUT', 120), ('LRATE', 50), ('LWAVE', 3), ('LD_PIT', 60)), IND, "alarme d'usine : anneau qui saute, résonance",
   "A3-A4, tenu", "S97")
IN("ST METAL", (101, 90, 40, 20, 50, 30, 40, 90), (0, 62, 0, 50), 0, 0, (30, 0, 40, 60), (("IN_CUT", 110),), IND,
   "stab métallique : FM inharmonique, anneau et résonance", "accords A2-A4", "S29")
IN("PA RING", (64, 0, 90, 0, 20, 0, 60, 40), (100, 90, 127, 110), 0, 0, (0, 70, 30, 115),
   (("IN_CUT", 80), ("LRATE", 20), ("LD_SHP", 20)), IND, "nappe en anneau, creuse et mouvante", "accords A2-A4",
   "S83")
IN("PC BELL IND", (88, 60, 0, 0, 0, 20, 50, 0), (0, 80, 0, 70), 0, 0, (10, 0, 30, 60), (("IN_CUT", 127),), IND,
   "cloche industrielle (FM pure)", "C4-C5", "S98")
IN("PC KICK ZAP", (64, 30, 0, 0, 40, 127, 40, 0), (0, 62, 0, 50), 0, 0, (60, 0, 0, 10), (("IN_CUT", 110),), HTK,
   "kick zap : la hauteur tombe de 4 octaves très vite", "C1-C2", "S110")
IN("DR FACTORY", (55, 80, 40, 80, 60, 0, 60, 60), (110, 90, 127, 110), 0, 0, (40, 30, 40, 120),
   (("IN_CUT", 70), ("LRATE", 70), ("LD_AMP", 60)), IND, "usine : machine qui ronronne et cogne", "A1-A3, tenu",
   "S83")
IN("BS RING IND", (32, 0, 80, 0, 40, 0, 60, 40), (0, 64, 100, 24), 0, 1, (40, 0, 5, 10), (("IN_CUT", 90),), IND,
   "basse en anneau (une octave dessous) métallique", "A1-A2", "S46")
IN("LD FM IND", (77, 100, 0, 0, 40, 0, 60, 60), (0, 70, 115, 40), 0, 1, (40, 0, 40, 30),
   (("IN_CUT", 110), ("LRATE", 60), ("LD_SHP", 30)), IND, "lead FM métallique dont l'indice ondule", "A3-A5", "S60")
# ---------------------------------------------------------------------------------------------------------- FM6
FM("ST FM BELL", (0, 0, 10, 0, 0, 0, 30, 1), (0, 0, 127, 0), 0, 0, (0, 40, 40, 60), (), MEL,
   "accord de cloches FM (patch BELL)", "accords A3-A5", "S29")
FM("BS FM BRASS", (0, 4, 10, 0, 0, 0, 0, 3), (0, 0, 127, 0), 0, 1, (30, 0, 5, 10), (), DET,
   "basse brass FM (patch BRASS dans le grave)", "A1-A2", "S46")
FM("BS FM PLUCK", (0, 0, 20, 0, -20, 0, 0, 7), (0, 0, 127, 0), 0, 1, (10, 0, 10, 10), (), MIN,
   "basse pincée FM (patch PLUCK dans le grave)", "A1-A2", "S46")
FM("PA FM WIDE", (0, 0, -30, 0, 50, 0, 127, 4), (0, 0, 127, 0), 0, 0, (0, 100, 30, 115), (("SPRD", 127),), MEL,
   "nappe FM très large (patch PAD, désaccord max)", "accords A3-A5", "S80")
FM("FX FM CHIRP", (0, 7, 63, 16, -64, 7, 0, 1), (0, 0, 127, 0), 0, 0, (20, 0, 60, 60), (), IND,
   "pépiement FM : rapport et indice au maximum, très court", "A3-A6", "S96")
FM("PL FM EP", (0, 0, 0, 0, -40, 0, 30, 0), (0, 0, 127, 0), 0, 0, (0, 50, 60, 60), (), DUB,
   "pluck de piano électrique FM, doux", "A3-A5", "S01")
# -------------------------------------------------------------------------------------------------------- PHASE
PH("LD PD SYNC", (7, 5, 100, 70, 0, 0, 0, 30), (0, 70, 110, 40), 0, 1, (30, 0, 40, 30), (), HTK,
   "lead CZ façon sync (RTRP), mordant", "A3-A5", "S13")
PH("BS PD SAW", (0, 0, 70, 90, 0, 0, 60, 0), (0, 56, 30, 20), 0, 1, (20, 0, 5, 5), (), TE,
   "basse scie CZ à l'attaque qui claque", "A1-A2", "S44")
PH("BS PD RING", (1, 0, 60, 60, 20, 1, 60, 0), (0, 60, 60, 24), 0, 1, (30, 0, 5, 5), (), IND,
   "basse CZ en anneau, métallique", "A1-A2", "S46")
PH("ST PD BELL", (6, 0, 90, 60, 0, 0, 0, 0), (0, 64, 0, 56), 0, 0, (0, 40, 50, 60), (), MEL,
   "accord de cloches CZ", "accords A3-A5", "S29")
PH("PA PD WIDE", (0, 0, 40, 20, 30, 0, 0, 0), (100, 90, 120, 110), 0, 0, (0, 110, 30, 110), (("SPRD", 127),), HTR,
   "nappe CZ désaccordée, large", "accords A2-A4", "S80")
PH("PA PD RESO", (5, 0, 30, 0, 0, 0, 0, 0), (100, 90, 120, 110), 0, 0, (0, 90, 30, 115),
   (("LRATE", 12), ("LD_SHP", 40)), IND, "nappe CZ résonante (RSAW) dont la résonance erre", "accords A2-A4", "S81")
PH("FX PD ZAP", (5, 0, 127, 64, 0, 0, 0, 60), (0, 70, 0, 60), 0, 0, (30, 0, 90, 50), (("ED_PIT", 63),), DET,
   "zap CZ : résonance et hauteur qui tombent", "A3-A5", "S96")
PH("PL PD PULSE", (2, 0, 100, 60, 0, 0, 0, 0), (0, 54, 0, 46), 0, 0, (0, 20, 60, 40), (), TE,
   "pluck pulse CZ, nasal", "A3-A5", "S12")
PH("DR PD RESO", (5, 5, 40, 0, 0, 0, 40, 40), (110, 90, 127, 110), 0, 0, (20, 40, 20, 120),
   (("LRATE", 4), ("LD_SHP", 50)), IND, "drone CZ résonant, très lent", "A1-A3, tenu", "S82")
# --------------------------------------------------------------------------------------------------------- TRIO
TR("BS TRIO NTCH", (0, 0, -12, 10, 3, 60, 80, 20), (0, 52, 0, 20), 20, 1, (20, 0, 10, 5), (), MIN,
   "basse au coupe-bande (creuse, phasée)", "A1-A2", "S44")
TR("ST TRIO MAJ7", (4, 4, 11, 4, 0, 110, 20, 64), (0, 72, 30, 60), 20, 0, (0, 100, 60, 70), (), DET,
   "accord majeur 7 d'une touche (triangles), Detroit", "A3-A5, une note", "S20")
TR("PA TRIO RING", (13, 0, 12, 20, 1, 70, 60, 64), (100, 90, 120, 110), 0, 0, (0, 70, 30, 115),
   (("LRATE", 16), ("LD_FLT", 20)), IND, "nappe en anneau, passe-bande qui bouge", "accords A2-A4", "S83")
TR("PL TRIO 5TH", (1, 7, 12, 0, 0, 100, 10, 40), (0, 64, 0, 60), 10, 0, (0, 60, 100, 60), (), MIN,
   "pluck en quintes d'une touche", "A3-A5", "S12")
TR("FX TRIO NOIS", (8, 0, 0, 0, 1, 60, 90, 64), (0, 80, 60, 80), 60, 0, (20, 40, 40, 90),
   (("LRATE", 30), ("LD_FLT", 30)), IND, "souffle chip passe-bande qui balaie", "A3, tenu", "S92")
TR("DR TRIO NOIS", (7, 12, 0, 20, 1, 50, 100, 64), (110, 90, 127, 110), 0, 0, (20, 40, 30, 120),
   (("LRATE", 8), ("LD_FLT", 30)), IND, "drone pulse + bruit au filtre très résonant", "A2-A3, tenu", "S82")
# --------------------------------------------------------------------------------------------------------- PHYS
PY("PL KOTO", (3, 100, 110, 40, 30, 110, 0, 40), (0, 100, 127, 50), 0, 0, (0, 20, 50, 60), (), MEL,
   "koto : corde pincée + cordes sympathiques en quinte", "A3-A5", "S10")
PY("PL HARP", (1, 64, 110, 100, 30, 100, 0, 0), (0, 100, 127, 80), 0, 0, (0, 30, 40, 80), (), MEL,
   "harpe : corde brillante et longue", "A3-A5", "S10")
PY("PL PIZZ", (1, 64, 80, 20, 40, 127, 0, 30), (0, 100, 127, 24), 0, 0, (0, 20, 40, 50), (), TE,
   "pizzicato : corde très amortie", "A3-A5", "S10")
PY("LD BLOW", (0, 70, 70, 100, 20, 80, 90, 0), (30, 100, 127, 60), 0, 1, (0, 30, 50, 60), (("GLIDE", 20),), DUB,
   "lead soufflé : lames modales tenues par le souffle", "A3-A5", "S68")
PY("PA SYMP", (3, 60, 70, 120, 64, 60, 80, 0), (80, 100, 127, 100), 0, 0, (0, 40, 30, 115), (), MEL,
   "nappe de cordes sympathiques qui résonnent", "accords A3-A5", "S85")
PY("PC TABLA", (2, 127, 90, 60, 110, 110, 60, 50), (0, 100, 127, 30), 0, 0, (0, 0, 10, 30), (), HG,
   "tabla : peau chargée, la hauteur qui plie", "C3-C4", "S116")
PY("PC STEELDRM", (0, 72, 100, 80, 20, 110, 0, 20), (0, 100, 127, 60), 0, 0, (0, 20, 30, 50), (), DUB,
   "steel drum modal", "C4-C5", "S116")
PY("PC WOODBLK", (0, 64, 100, 20, 30, 127, 0, 70), (0, 100, 127, 16), 0, 0, (0, 0, 20, 30), (), MIN,
   "wood block très court", "C4-C6", "S119")
PY("DR SITAR", (3, 0, 80, 120, 40, 90, 100, 0), (60, 100, 127, 110), 0, 0, (10, 30, 30, 120), (), DUB,
   "drone de sitar : chevalet bourdonnant, archet", "A1-A3, tenu", "S82")
PY("FX METAL DRP", (0, 127, 127, 90, 10, 127, 0, 50), (0, 100, 127, 80), 0, 0, (20, 0, 60, 110),
   (("ED_PIT", 40),), IND, "goutte de métal : modes très inharmoniques, hauteur qui tombe", "A3-A5", "S98")
# -------------------------------------------------------------------------------------------------------- NOISE
NZ("FX SWEEP DN", (0, 30, 20, 100, 0, 0, 0, 0), (0, 90, 127, 60), 0, 0, (0, 50, 30, 90),
   (("LRATE", 2), ("LD_FLT", -63), ("LWAVE", 2)), HTR, "sweep de bruit qui descend", "A3, tenu 4 mesures", "S91")
NZ("FX WIND HOWL", (0, 40, 70, 120, 64, 0, 100, 0), (90, 90, 127, 100), 0, 0, (0, 40, 30, 110),
   (("LRATE", 40), ("LD_FLT", 30)), IND, "vent qui hurle : passe-bande résonant qui erre vite", "A3-A5, tenu", "S92")
NZ("FX HELICO", (0, 60, 60, 30, 0, 0, 0, 0), (0, 70, 127, 60), 0, 0, (10, 0, 20, 60),
   (("LRATE", 100), ("LD_AMP", 120)), IND, "hélicoptère : bruit haché très vite", "A3, tenu", "S100")
NZ("FX STEAM", (0, 0, 120, 0, 0, 0, 30, 0), (20, 80, 40, 80), 0, 0, (0, 20, 20, 70),
   (("ITYPE", 9), ("IA", 100), ("IC", 127)), IND, "jet de vapeur : bruit blanc passe-haut qui siffle", "A3, un coup",
   "S92")
NZ("PC HAT OPEN", (0, 0, 127, 10, 0, 0, 0, 0), (0, 64, 0, 60), 0, 0, (0, 0, 20, 30),
   (("ITYPE", 9), ("IA", 115), ("IC", 127)), TE, "charley ouvert de bruit", "C4-C6", "S113")
NZ("PC RIDE META", (3, 0, 127, 30, 127, 60, 20, 0), (0, 80, 0, 80), 0, 0, (0, 30, 20, 50), (), DET,
   "ride métallique (META), longue", "C5-C6", "S114")
NZ("PC SNR LFSR", (2, 0, 100, 20, 64, 90, 0, 0), (0, 52, 0, 48), 0, 0, (20, 0, 10, 40), (), MIN,
   "snare 8 bits (registre)", "C3-C4", "S120")
NZ("LD META", (3, 0, 100, 40, 127, 30, 0, 0), (0, 70, 110, 40), 10, 1, (30, 0, 40, 40), (("GLIDE", 14),), IND,
   "lead métallique (META), accordé, glide", "A3-A5", "S60")
NZ("PA META", (3, 0, 80, 60, 127, 60, 40, 0), (100, 90, 127, 110), 0, 0, (0, 70, 30, 115),
   (("LRATE", 14), ("LD_FLT", 20)), IND, "nappe métallique accordée (META)", "accords A2-A4", "S83")
NZ("BS LFSR", (2, 0, 100, 0, 127, 80, 0, 40), (0, 60, 60, 20), 0, 1, (20, 0, 5, 5), (), MIN,
   "basse 8 bits (registre court)", "A1-A2", "S44")
NZ("PL DUST", (1, 0, 110, 90, 127, 90, 0, 0), (0, 56, 0, 50), 0, 0, (0, 20, 70, 60), (), MIN,
   "pluck de poussière accordée (impulses dans un filtre résonant)", "A3-A5", "S11")


# ========================================================================================================= third set
# (the INSERT: ITYPE 3 FOLD (A DRIVE B TONE C LEVEL), 5 CRUSH (A BITS: v / 8 + 1 bits, B RATE, C LPF), 6 PHASR, 7 FLANG
#  (A RATE B DEPTH C FDBK), 9 FILTR (A CUTOFF B RES C SHAPE: 0 LP .. 64 BP .. 127 HP); the matrix: SRC 1 LFO 9 ENV2,
#  DST 1 PITCH 2 CUT 5 PAN)
# ------------------------------------------------------------------------------------------------------ ANALOG
AN("PL BITPLK", (1, 0, 0, 0, 100, 20, 0, 64), (0, 66, 0, 60), 20, 0, (0, 0, 90, 50),
   (('ITYPE', 5), ('IA', 16), ('IB', 64), ('IC', 127)), MIN, "pluck carré en 4 bits", "A3-A5", "S12")
AN("ST PHASER", (0, 14, 64, 0, 70, 40, 10, 64), (0, 62, 20, 48), 30, 0, (0, 20, 50, 60),
   (("ITYPE", 6), ("IA", 50), ("IB", 120), ("IC", 100)), TE, "stab au phaser lent", "accords A2-A4", "S26")
AN("PA FLANGE", (0, 16, 64, 0, 70, 20, 0, 64), (90, 90, 120, 100), 0, 0, (0, 0, 30, 110),
   (("ITYPE", 7), ("IA", 30), ("IB", 120), ("IC", 110)), TE, "nappe au flanger qui monte et descend (jet)",
   "accords A2-A4", "S80")
AN("BS FOLD", (2, 0, 0, 0, 127, 0, 0, 64), (0, 64, 110, 24), 0, 1, (0, 0, 5, 5),
   (('ITYPE', 3), ('IA', 110), ('IB', 80), ('IC', 127)), HT, "basse triangle repliée par l'insert FOLD (grondement)",
   "A1-A2", "S47")
AN("PA AUTOPAN", (0, 8, 64, 0, 60, 10, 0, 64), (80, 90, 120, 100), 0, 0, (0, 0, 30, 100),
   (('M1SRC', 1), ('M1DST', 5), ('M1AMT', 63), ('LRATE', 40)), MEL, "nappe de scies qui tourne de gauche à droite (LFO sur le PAN)",
   "accords A3-A5", "S85")
AN("LD THIN HP", (0, 10, 64, 0, 127, 0, 30, 64), (0, 70, 110, 40), 0, 1, (20, 0, 50, 40),
   (("ITYPE", 9), ("IA", 90), ("IB", 70), ("IC", 127)), HTR, "lead fin, coupé dans le bas par un passe-haut résonant",
   "A3-A5", "S62")
AN("FX BITFALL", (0, 30, 64, 0, 100, 20, 30, 64), (0, 110, 0, 100), 30, 0, (20, 0, 60, 80),
   (("ED_PIT", 63), ("ITYPE", 5), ("IA", 16), ("IB", 30)), IND, "chute en 3 bits : la hauteur tombe dans le bitcrush",
   "A3-A5, un coup", "S99")
AN("BS DROP", (3, 0, 0, 0, 127, 0, 40, 0), (0, 80, 60, 40), 0, 1, (30, 0, 5, 10),
   (("M1SRC", 9), ("M1DST", 1), ("M1AMT", 63), ("EG2_DEC", 76)), TE,
   "basse qui plonge : ENV 2 sur la hauteur, chaque note tombe d'une octave", "A1-A2", "S45")
AN("PL BOING", (2, 0, 0, 0, 90, 80, 0, 64), (0, 54, 0, 44), 20, 0, (0, 0, 50, 40),
   (("M1SRC", 9), ("M1DST", 1), ("M1AMT", -40), ("EG2_DEC", 56)), HG, "pluck « boing » : la hauteur remonte vite",
   "A3-A5", "S43")
AN("FX COMPUTER", (2, 0, 0, 0, 100, 20, 0, 64), (0, 70, 127, 40), 0, 0, (0, 0, 60, 40),
   (('LRATE', 100), ('LWAVE', 4), ('LD_PIT', 63), ('LSYNC', 8)), DET,
   "ordinateur des années 70 : la hauteur saute au hasard à chaque 1/16", "A3-A4, tenu", "S99")
AN("DR NOISE RES", (2, 0, 0, 127, 70, 120, 0, 127), (110, 90, 127, 110), 0, 0, (0, 40, 30, 120),
   (("LRATE", 8), ("LD_FLT", 10)), IND, "drone de bruit accordé par le filtre résonant", "A2-A4, tenu", "S82")
AN("ST FOLD SQR", (1, 0, 0, 0, 60, 20, 0, 64), (0, 64, 30, 50), 20, 0, (0, 20, 50, 50),
   (('ITYPE', 3), ('IA', 127), ('IB', 60), ('IC', 127)), HT, "stab carré replié, métallique", "accords A2-A4", "S28")
AN("PC CRUSH HAT", (2, 0, 0, 127, 127, 0, 0, 127), (0, 40, 0, 36), 0, 0, (0, 0, 20, 30),
   (("ITYPE", 5), ("IA", 40), ("IB", 20), ("IC", 127)), HG, "charley écrasé (bruit en 6 bits, 1,4 kHz)", "C4-C6",
   "S111")
# ---------------------------------------------------------------------------------------------------- SUPERSAW
SS("LD SS FOLD", (60, 90, 110, 20, 80, 40, 64, 20), (0, 70, 110, 40), 0, 0, (100, 30, 40, 40),
   (('SPRD', 80), ('DTYPE', 2)), HTK, "supersaw replié par la DIST de la piste (type FOLD) : grinçant, métallique", "A3-A5",
   "S64")
SS("DR SS FLANGE", (127, 127, 60, 30, 64, 0, 40, 30), (110, 90, 127, 110), 0, 0, (0, 0, 20, 127),
   (("SPRD", 127), ("ITYPE", 7), ("IA", 10), ("IB", 127)), IND, "drone supersaw au flanger très lent", "A1-A3, tenu",
   "S82")
# ------------------------------------------------------------------------------------------------------ HOOVER
HV("ST HOOV FLNG", (40, 60, 90, 44, 96, 20, 30, 20), (0, 66, 0, 50), 10, 0, (10, 0, 50, 60),
   (("ITYPE", 7), ("IA", 60), ("IB", 110), ("IC", 120)), RAV, "stab hoover au flanger", "accords A3-A5", "S24")
HV("BS HOOV FOLD", (30, 50, 80, 30, 80, 20, 90, 30), (0, 62, 80, 24), 0, 1, (20, 0, 10, 5),
   (("ITYPE", 3), ("IA", 90), ("IB", 60)), HTK, "basse hoover repliée", "A1-A2", "S51")
HV("FX HOOV GATE", (60, 80, 110, 60, 110, 20, 50, 40), (0, 80, 127, 60), 0, 0, (30, 80, 40, 60),
   (("LRATE", 80), ("LSYNC", 10), ("LD_AMP", 127), ("SPRD", 110)), HTK, "hoover haché en 1/32 (roulement)",
   "A2-A4, tenu", "S63")
# -------------------------------------------------------------------------------------------------------- ACID
AC("BS 303 CRUSH", (2, 40, 100, 80, 50, 90, 0, 45), (0, 100, 127, 10), 0, 1, (10, 0, 30, 10),
   (('ITYPE', 5), ('IA', 24), ('IB', 30), ('IC', 120)), ACI, "303 écrasé (6 bits) : acid lo-fi", "A1-A2", "S42")
AC("BS 303 PHASR", (0, 30, 100, 80, 60, 80, 0, 45), (0, 100, 127, 12), 0, 1, (0, 0, 30, 20),
   (("ITYPE", 6), ("IA", 60), ("IB", 120), ("IC", 110)), ACI, "303 au phaser", "A1-A2", "S42")
AC("LD 303 FOLD", (0, 50, 110, 80, 60, 100, 0, 45), (0, 100, 127, 16), 0, 1, (20, 0, 60, 30),
   (("ITYPE", 3), ("IA", 110), ("IB", 90)), ACI, "lead acid replié : la résonance se casse en harmoniques", "A2-A4",
   "S66")
AC("BS 303 GATE", (1, 30, 100, 80, 80, 80, 0, 45), (0, 100, 127, 12), 0, 1, (10, 0, 30, 10),
   (("LRATE", 80), ("LSYNC", 10), ("LD_AMP", 100)), ACI, "303 haché en 1/32", "A1-A2", "S42")
AC("PL 303 FLNG", (0, 60, 110, 60, 20, 90, 0, 20), (0, 100, 127, 8), 0, 1, (0, 0, 70, 50),
   (("ITYPE", 7), ("IA", 40), ("IB", 120), ("IC", 120)), ACI, "pluck 303 au flanger, delay", "A2-A4", "S02")
# -------------------------------------------------------------------------------------------------------- MINI
MI("LD MG MEGA", (90, 50, 40, 0, 70, 90, 0, 0), (0, 70, 120, 50), 0, 1, (0, 50, 50, 50),
   (("ITYPE", 5), ("IA", 48), ("IB", 60), ("IC", 120)), DET, "méga-lead (patch MEGA LEAD), un peu écrasé", "A3-A5",
   "S67", slot=22)
MI("PA MG FLANGE", (60, 20, 30, 50, 80, 90, 0, 0), (90, 90, 120, 100), 0, 0, (0, 0, 30, 110),
   (("ITYPE", 7), ("IA", 20), ("IB", 120), ("IC", 110)), TE, "pad chaud au flanger (patch WARM PAD)", "accords A2-A4",
   "S80", slot=41)
MI("ST MG FUNK", (80, 60, 70, 0, 40, 0, 0, 0), (0, 54, 0, 40), 0, 0, (10, 0, 40, 40),
   (("ITYPE", 6), ("IA", 70), ("IB", 120)), HG, "stab funk au phaser (patch FUNK ARP)", "accords A3-A5", "S23", slot=63)
MI("BS MG FOLD", (40, 30, 40, 0, 70, 80, 0, 0), (0, 64, 110, 24), 0, 1, (0, 0, 5, 5),
   (("ITYPE", 3), ("IA", 100), ("IB", 70)), HT, "sub Moog replié (patch SUB BASS)", "A1-A2", "S47", slot=32, bass=1)
MI("PL MG NYLON", (90, 20, 30, 0, 60, 0, 0, 0), (0, 70, 0, 66), 0, 0, (0, 20, 40, 50), (), MEL,
   "guitare nylon synthétique (patch NYLON)", "A3-A5", "S10", slot=64)
MI("FX MG HELICO", (60, 50, 50, 0, 80, 80, 0, 0), (0, 80, 127, 60), 0, 0, (10, 0, 30, 60), (), IND,
   "hélicoptère (patch HELICOPTER)", "A2-A3, tenu", "S100", slot=97)
MI("FX SIREN MG", (100, 60, 40, 0, 80, 80, 40, 0), (20, 80, 127, 60), 0, 0, (40, 0, 60, 80), (), RAV,
   "sirène (patch SIREN)", "A3-A4, tenu", "S97", slot=93)
# ------------------------------------------------------------------------------------------------------- INDUS
IN("PC SNARE IND", (64, 0, 0, 127, 40, 40, 50, 0), (0, 52, 0, 48), 0, 0, (80, 0, 10, 80), (('IN_CUT', 90), ('DTYPE', 1)), IND,
   "snare industrielle : bruit + sinus replié", "C3-C4", "S117")
IN("PC HAT IND", (127, 100, 100, 80, 40, 0, 40, 0), (0, 36, 0, 30), 0, 0, (20, 0, 10, 30), (("IN_CUT", 127),), IND,
   "charley métallique : FM et anneau très aigus", "C4-C6", "S117")
IN("PA STEEL", (120, 30, 20, 0, 20, 0, 60, 60), (100, 90, 127, 110), 0, 0, (0, 70, 30, 115),
   (("IN_CUT", 100), ("LRATE", 14), ("LD_SHP", 20)), IND, "nappe d'acier : FM aiguë inharmonique, résonance",
   "accords A2-A4", "S83")
IN("FX FEEDBACK", (64, 0, 0, 20, 0, 0, 60, 127), (60, 90, 127, 80), 0, 0, (60, 0, 40, 90),
   (("IN_CUT", 100), ("LRATE", 30), ("LD_FLT", 30)), IND, "larsen : résonance au maximum qui erre", "A3-A5, tenu",
   "S98")
IN("DR FM IND", (90, 90, 0, 0, 40, 0, 60, 30), (110, 90, 127, 110), 0, 0, (20, 40, 30, 120),
   (("IN_CUT", 80), ("LRATE", 6), ("LD_SHP", 60)), IND, "drone FM dont l'indice ondule très lentement", "A1-A3, tenu",
   "S82")
# --------------------------------------------------------------------------------------------------------- FM6
FM("ST FM MARIMB", (0, 0, 20, 0, 0, 0, 30, 5), (0, 0, 127, 0), 0, 0, (0, 40, 50, 60), (), MIN,
   "accord de marimba FM", "accords A3-A5", "S08")
FM("PA FM BELL", (0, 0, 0, 2, 63, 0, 80, 1), (0, 0, 127, 0), 0, 0, (0, 80, 30, 120), (), MEL,
   "nappe de cloches : l'enveloppe des modulateurs ralentie au maximum", "accords A3-A5", "S83")
FM("LD FM BELL", (0, 2, 20, 0, 40, 0, 20, 1), (0, 0, 127, 0), 0, 1, (0, 30, 60, 60), (("GLIDE", 24),), DUB,
   "lead de cloche FM, glide", "A3-A5", "S68")
FM("PC FM TOM", (0, 0, 10, -8, -40, 0, 0, 2), (0, 0, 127, 0), 0, 0, (20, 0, 10, 50), (), TE,
   "tom FM (patch BASS, rapport bas)", "C2-C3", "S117")
FM("FX FM SWEEP", (0, 5, 63, 9, 63, 0, 60, 4), (0, 0, 127, 0), 0, 0, (10, 60, 40, 110), (), IND,
   "balayage FM : indice au maximum, modulateurs très lents", "A2-A4, tenu", "S92")
FM("BS FM ORGAN", (0, 2, 20, 0, 0, 0, 0, 6), (0, 0, 127, 0), 0, 1, (30, 0, 5, 10), (), HG,
   "basse d'orgue (tirettes dans le grave)", "A1-A2", "S44")
# -------------------------------------------------------------------------------------------------------- PHASE
PH("PL PD FOLD", (0, 0, 80, 60, 0, 0, 0, 90), (0, 56, 0, 46), 0, 0, (10, 20, 60, 40), (), HT,
   "pluck CZ replié, agressif", "A3-A5", "S13")
PH("ST PD RING", (1, 2, 70, 50, 20, 1, 0, 0), (0, 62, 0, 50), 0, 0, (10, 30, 50, 60), (), IND,
   "stab CZ en anneau", "accords A2-A4", "S29")
PH("PA PD SAW", (0, 1, 70, 10, 0, 0, 30, 0), (100, 90, 120, 110), 0, 0, (0, 100, 30, 110),
   (('LRATE', 16), ('LD_SHP', 30)), TE, "nappe de scies CZ dont la brillance respire", "accords A2-A4", "S80")
PH("FX PD SWEEP", (5, 0, 0, 0, 0, 0, 0, 40), (0, 90, 127, 60), 0, 0, (20, 40, 40, 100),
   (("LRATE", 4), ("LWAVE", 2), ("LD_SHP", 63)), IND, "balayage de résonance CZ très lent", "A2-A4, tenu", "S92")
PH("DR PD CHOIR", (3, 6, 50, 20, 0, 0, 40, 0), (110, 90, 127, 110), 0, 0, (0, 70, 30, 120),
   (("LRATE", 6), ("LD_SHP", 20)), MEL, "chœur CZ (double sinus), très lent", "A2-A4, tenu", "S87")
# --------------------------------------------------------------------------------------------------------- TRIO
TR("ST TRIO NTCH", (0, 3, 7, 10, 3, 70, 90, 64), (0, 62, 20, 50), 30, 0, (0, 40, 50, 60), (), TE,
   "accord mineur d'une touche au coupe-bande", "A2-A4, une note", "S27")
TR("BS TRIO HP", (0, 12, -12, 8, 2, 30, 60, 64), (0, 56, 60, 20), 30, 1, (20, 0, 10, 5), (), MIN,
   "basse au passe-haut : creuse, claquante", "A1-A2", "S44")
TR("PA TRIO PLS", (1, 12, 7, 20, 0, 80, 30, 30), (90, 90, 120, 100), 0, 0, (0, 90, 30, 110),
   (("LRATE", 30), ("LD_SHP", 40)), MEL, "nappe de pulses étroites (octave et quinte)", "accords A3-A5", "S88")
TR("FX TRIO RING", (12, 0, 31, 0, 0, 100, 40, 64), (0, 80, 60, 80), 0, 0, (20, 0, 60, 80),
   (("LRATE", 60), ("LD_PIT", 30)), IND, "sonnerie en anneau qui vacille", "A3-A5", "S97")
TR("PC TRIO TOM", (4, 0, 0, 0, 0, 100, 20, 64), (0, 56, 0, 50), 0, 0, (0, 0, 10, 40), (("ED_PIT", 63),), TE,
   "tom chip : triangle dont la hauteur tombe", "C2-C4", "S117")
# -------------------------------------------------------------------------------------------------------- GRAIN
GR("PA GRN SAX", (3, 20, 110, 70, 0, 60, 10, 100), (100, 90, 120, 110), 0, 0, (0, 60, 30, 115), (), DUB,
   "nappe de sax granulaire, à la note", "accords A2-A4", "S85")
GR("DR GRN FLUTE", (2, 80, 127, 40, -24, 100, 40, 70), (110, 90, 127, 110), 0, 0, (0, 60, 30, 127), (), MEL,
   "drone de flûte une octave dessous, éparpillé", "A2-A4, tenu", "S82")
GR("FX GRN REV", (0, 100, 90, 80, 0, 60, 127, 90), (60, 90, 127, 100), 0, 0, (0, 40, 50, 110), (), IND,
   "piano déréglé : grains au désaccord maximal", "A3-A5, tenu", "S99")
GR("PL GRN SAX", (3, 10, 20, 70, 0, 10, 10, 110), (0, 58, 0, 48), 0, 0, (10, 20, 60, 50), (), HG,
   "pluck de sax granulaire", "A3-A5", "S11")
GR("ST GRN FLUTE", (2, 10, 50, 100, 12, 30, 0, 110), (0, 60, 0, 50), 0, 0, (0, 40, 60, 70), (), DUB,
   "stab de flûte granulaire, une octave au-dessus", "accords A3-A5", "S21")
# --------------------------------------------------------------------------------------------------------- PHYS
PY("PC BELL PHYS", (0, 75, 110, 100, 20, 110, 0, 20), (0, 100, 127, 80), 0, 0, (0, 20, 30, 70), (), DET,
   "cloche modale accordée", "C4-C5", "S119")
PY("PC SNARE PHY", (2, 0, 127, 30, 127, 127, 0, 127), (0, 100, 127, 30), 0, 0, (20, 0, 10, 40), (), TE,
   "snare de peau : membrane frappée au bord, beaucoup de frappe", "C3-C4", "S120")
PY("BS MEMB 808", (2, 0, 30, 100, 0, 110, 60, 0), (0, 100, 127, 80), 0, 1, (30, 0, 0, 10), (), DET,
   "basse de peau à la 808 : membrane grave dont la hauteur tombe", "A1-A2", "S45")
PY("LD STR BOW", (1, 70, 100, 120, 20, 127, 127, 0), (40, 100, 127, 60), 0, 1, (0, 40, 40, 70),
   (("LRATE", 80), ("LD_PIT", 8), ("LFADE", 60)), MEL, "violon synthétique : corde frottée, vibrato", "A4-A6", "S68")
PY("PC CLAVES", (0, 64, 127, 10, 20, 127, 0, 50), (0, 100, 127, 10), 0, 0, (0, 0, 10, 20), (), HG,
   "claves : deux bâtons, très court et aigu", "C5-C6", "S119")
# -------------------------------------------------------------------------------------------------------- NOISE
NZ("FX SCRATCH", (1, 0, 90, 60, 64, 110, 100, 30), (0, 70, 60, 50), 0, 0, (20, 0, 20, 40),
   (("LRATE", 90), ("LD_FLT", 30)), DUB, "grattement de vinyle", "A3, tenu", "S100")
NZ("FX TAPE NOIS", (0, 127, 40, 0, 0, 0, 40, 0), (40, 90, 127, 90), 0, 0, (0, 30, 20, 60), (), DUB,
   "souffle de bande (bruit brun)", "A3, tenu", "S100")
NZ("PA WIND CHRD", (0, 40, 70, 127, 127, 0, 40, 0), (100, 90, 127, 110), 0, 0, (0, 60, 30, 120), (), TE,
   "accord de vent : bruit dans un passe-bande très étroit, accordé", "accords A3-A5", "S87")
NZ("BS NOISE RES", (0, 60, 80, 127, 127, 0, 0, 0), (0, 64, 110, 24), 0, 1, (30, 0, 5, 10), (), IND,
   "basse de bruit filtré : passe-bande étroit sur la note", "A1-A2", "S52")
NZ("LD WHSTL NZ", (0, 0, 90, 127, 127, 0, 30, 0), (20, 70, 120, 50), 0, 1, (0, 20, 50, 60),
   (("LRATE", 84), ("LD_PIT", 8), ("GLIDE", 20)), DUB, "sifflet de bruit : passe-bande au maximum, vibrato",
   "A4-A5", "S68")
# --------------------------------------------------------------------------------------------------------- DRUM
DM("KT PERC DRY", (11, 70, 80, 40, 90, 100, 0, 0), (0, 100, 127, 100), 0, 0, (0, 0, 0, 0), (), HG,
   "percussions sèches (kit PERC), sans effet", "boucles", "S116")
DM("KT DEEP VERB", (9, 48, 70, 64, 50, 100, 0, 20), (0, 100, 127, 100), 0, 0, (10, 0, 30, 110), (), TE,
   "kit profond et sombre, noyé dans la reverb", "beat 4/4 lent", "S113")
DM("KT HARD 909", (5, 60, 110, 50, 120, 127, 0, 110), (0, 100, 127, 100), 0, 0, (100, 0, 0, 20),
   (("DTYPE", 2),), HT, "kit hard techno (modèle « 10 ») replié", "beat 145-155", "S110")
# ------------------------------------------------------------------------------------------------------- DRUM-X
DX("KT DX CLICK2", (90, 70, 100, 20, 80, 30, 60, 0), (0, 100, 127, 100), 0, 0, (0, 0, 40, 30), (), MIN,
   "kit DRUM-X de clicks repliés", "beat sec", "S118", kit=3)
DX("KT DX LOW", (60, 40, 50, 64, 50, 20, 0, 60), (0, 100, 127, 100), 0, 0, (40, 0, 10, 60), (), IND,
   "kit DRUM-X industriel accordé bas", "beat 130-140", "S117", kit=4)
DX("KT DX FM", (80, 70, 100, 50, 60, 127, 0, 20), (0, 100, 127, 100), 0, 0, (10, 0, 30, 40), (), MIN,
   "kit DRUM-X à la FM au maximum", "beat cassé", "S99", kit=6)
DX("KT DX BOOM", (40, 50, 40, 64, 40, 0, 0, 40), (0, 100, 127, 100), 0, 0, (20, 0, 20, 90), (), DUB,
   "kit DRUM-X grave aux queues très longues", "beat 4/4 lent", "S113", kit=2)
DX("KT DX TRBL2", (64, 80, 90, 50, 70, 20, 0, 30), (0, 100, 127, 100), 0, 0, (10, 0, 20, 30), (), HG,
   "kit DRUM-X tribal accordé haut", "boucles", "S116", kit=7)
DX("KT DX SCIFI", (100, 70, 110, 60, 80, 100, 40, 40), (0, 100, 127, 100), 0, 0, (20, 0, 50, 50), (), IND,
   "kit DRUM-X science-fiction (lasers FM)", "beat cassé", "S93", kit=8)


# ========================================================================================================= last set
PY("PC TOM PHYS", (2, 20, 60, 70, 20, 110, 40, 30), (0, 100, 127, 60), 0, 0, (10, 0, 10, 50), (), TE,
   "tom de peau grave, la hauteur qui tombe", "C2-C3", "S117")
PY("PC GLASS", (0, 90, 127, 60, 10, 110, 0, 40), (0, 100, 127, 50), 0, 0, (0, 20, 40, 60), (), MIN,
   "verre frappé : modes très aigus", "C5-C6", "S119")
PY("FX BOWL", (0, 72, 70, 127, 30, 90, 100, 0), (60, 100, 127, 110), 0, 0, (0, 30, 20, 120), (), MEL,
   "bol chantant : modes frottés qui tournent", "A2-A4, tenu", "S83")
PY("DR CAVE", (3, 20, 40, 127, 90, 80, 110, 0), (100, 100, 127, 120), 0, 0, (10, 30, 20, 127), (), IND,
   "drone de grotte : cordes sympathiques sombres, frottées", "A1-A2, tenu", "S82")
PY("PL ZITHER", (3, 80, 120, 70, 30, 110, 0, 20), (0, 100, 127, 60), 0, 0, (0, 30, 50, 70), (), MEL,
   "cithare : cordes sympathiques brillantes, pincées", "A3-A5", "S10")
NZ("FX RAINSTCK", (1, 30, 100, 40, 64, 100, 60, 0), (70, 90, 0, 80), 0, 0, (0, 20, 20, 60), (), DUB,
   "bâton de pluie : poussière qui monte et retombe", "A3, un coup", "S100")
NZ("FX BREATH", (0, 64, 60, 50, 0, 0, 70, 0), (100, 90, 0, 100), 0, 0, (0, 40, 20, 90), (), MEL,
   "souffle : bruit rose qui enfle et s'éteint", "A3, un coup", "S92")
NZ("DR DEEP NZ", (0, 127, 30, 60, 0, 0, 80, 0), (110, 90, 127, 110), 0, 0, (20, 30, 20, 120),
   (("LRATE", 4), ("LD_FLT", 20)), IND, "grondement de bruit brun très grave", "A1, tenu", "S82")
IN("FX MTL ROLL", (101, 100, 30, 20, 40, 0, 40, 40), (0, 80, 127, 60), 0, 0, (20, 0, 30, 80),
   (("IN_CUT", 110), ("LRATE", 80), ("LSYNC", 10), ("LD_AMP", 127)), IND, "roulement de tôle haché en 1/32",
   "A2-A4, tenu", "S117")
IN("PC CLAP IND", (64, 0, 0, 127, 60, 0, 30, 60), (0, 46, 0, 44), 0, 0, (40, 0, 10, 100), (("IN_CUT", 100),), IND,
   "clap industriel : bruit résonant, grande reverb", "C3-C4", "S120")
IN("DR TURBINE", (45, 40, 80, 40, 40, 0, 60, 60), (110, 90, 127, 110), 0, 0, (20, 40, 30, 115),
   (("IN_CUT", 90), ("LRATE", 3), ("LD_PIT", 20)), IND, "turbine : anneau dont la hauteur monte et descend lentement",
   "A1-A3, tenu", "S82")
FM("PC FM BELL", (0, 0, 20, 16, -30, 0, 0, 1), (0, 0, 127, 0), 0, 0, (0, 0, 30, 60), (), DET,
   "clochette FM très aiguë (rapport maximal)", "C4-C5", "S119")
FM("DR FM PAD", (0, 0, 20, 4, 40, 0, 127, 4), (0, 0, 127, 0), 0, 0, (10, 80, 30, 127),
   (("LRATE", 4), ("LD_PIT", 4)), IND, "drone FM désaccordé au maximum", "A1-A3, tenu", "S82")
TR("PC TRIO HAT", (8, 0, 0, 0, 2, 120, 30, 64), (0, 36, 0, 30), 0, 0, (0, 0, 10, 20), (), MIN,
   "charley chip : bruit au passe-haut", "C4-C6", "S113")
TR("DR TRIO DTN", (0, 0, 0, 50, 0, 50, 60, 64), (110, 90, 127, 110), 0, 0, (20, 60, 20, 120),
   (("LRATE", 6), ("LD_FLT", 30)), IND, "drone de trois scies au désaccord maximal", "A1-A3, tenu", "S82")
GR("FX GRN CRACK", (3, 64, 0, 40, 0, 127, 127, 127), (0, 80, 127, 50), 0, 0, (20, 0, 20, 40), (), IND,
   "craquements de sax minuscules et rares", "A3, tenu", "S100")
DX("KT DX DIRTY", (64, 60, 90, 60, 80, 20, 60, 127), (0, 100, 127, 100), 0, 0, (60, 0, 0, 20), (), HT,
   "kit DRUM-X sale : drive et repli", "beat 140-150", "S110", kit=1)
DX("KT DX WOOD", (40, 60, 40, 50, 20, 0, 0, 0), (0, 100, 127, 100), 0, 0, (0, 0, 20, 40), (), MIN,
   "kit DRUM-X boisé, presque sans bruit", "beat sec", "S118", kit=7)
DX("KT DX METAL2", (100, 90, 120, 40, 100, 60, 40, 60), (0, 100, 127, 100), 0, 0, (20, 0, 20, 30), (), IND,
   "kit DRUM-X industriel accordé haut", "beat 135-145", "S117", kit=4)
DM("KT TRIBAL", (11, 56, 70, 90, 70, 110, 1, 40), (0, 100, 127, 100), 0, 0, (20, 0, 20, 60), (), HG,
   "kit tribal (percussions accordées bas, reverb)", "boucles", "S116")
AN("FX UFO ANA", (3, 0, 0, 0, 127, 0, 0, 64), (60, 90, 127, 80), 0, 0, (0, 0, 70, 90),
   (("LRATE", 104), ("LD_PIT", 40), ("LFADE", 80)), DET, "soucoupe : sinus au vibrato rapide et profond", "A4-A5, tenu",
   "S96")


# ===================================================================================== the references (RECHERCHE-SONS)
REFS = [
    ("S01", "pluck FM"), ("S02", "pluck acid"), ("S03", "pluck supersaw"), ("S04", "pluck de techno mélodique"),
    ("S05", "blip de minimal berlinoise"), ("S06", "ping de dub techno"), ("S07", "pluck trance"),
    ("S08", "pluck mallet / marimba FM"), ("S09", "pluck cloche / métal"), ("S10", "corde pincée"),
    ("S11", "pluck bruité / tick"), ("S12", "arp chiptune / carré"), ("S13", "pluck hard-sync"),
    ("S14", "pluck d'orgue"), ("S15", "pluck hypnotique"),
    ("S20", "chord stab de Detroit"), ("S21", "accord de dub techno"), ("S22", "stab rave"),
    ("S23", "stab d'orgue house"), ("S24", "stab hoover"), ("S25", "stab brass Jupiter"),
    ("S26", "stab techno peak-time"), ("S27", "stab d'accords parallèles"), ("S28", "stab distordu"),
    ("S29", "stab métallique FM"), ("S30", "accords de piano rave"),
    ("S40", "rumble"), ("S41", "reese"), ("S42", "basse acid 303"), ("S43", "donk"),
    ("S44", "basse roulante / contretemps"), ("S45", "sub"), ("S46", "basse FM métallique"),
    ("S47", "basse schranz / saturée"), ("S48", "kick-basse hardtek"), ("S49", "basse psy roll"),
    ("S50", "basse Moog / « I Feel Love »"), ("S51", "basse hoover / PWM / sync"), ("S52", "basse de percussion accordée"),
    ("S60", "screech industriel"), ("S61", "lead schranz"), ("S62", "lead hard trance"), ("S63", "lead hoover"),
    ("S64", "lead saturé"), ("S65", "sirène"), ("S66", "lead acid / gabber"), ("S67", "lead techno old school"),
    ("S68", "lead une note"), ("S69", "lead criard"),
    ("S80", "nappe trance"), ("S81", "pad sombre"), ("S82", "drone"), ("S83", "texture métallique"),
    ("S84", "strings de Detroit"), ("S85", "pad d'ambient techno"), ("S86", "pad gaté trance"),
    ("S87", "chœur"), ("S88", "pad PWM Juno"),
    ("S90", "riser"), ("S91", "downlifter"), ("S92", "sweep de bruit"), ("S93", "laser"), ("S94", "impact"),
    ("S95", "sonar"), ("S96", "zap"), ("S97", "sirène / alarme"), ("S98", "clank métallique"),
    ("S99", "glitch"), ("S100", "radio / statique"),
    ("S110", "kick hard techno"), ("S111", "percussions schranz"), ("S112", "percussions hardgroove"),
    ("S113", "beat berlinois"), ("S114", "beat Detroit / 909"), ("S115", "kick gabber"),
    ("S116", "percussion tribale"), ("S117", "percussions industrielles"), ("S118", "clicks de minimal"),
    ("S119", "rim, clave, cowbell"), ("S120", "snare / clap de bruit"), ("S121", "kit 808 électro"),
]


# ================================================================== the techno presets of phases 2 and 3 that stay
class K:
    kept = True
    replace = None

    def __init__(s, eng, name, voice, use, notes, style, ref):
        s.eng, s.name, s.voice, s.use, s.notes, s.style, s.ref = eng, name, voice, use, notes, style, ref


KEPT_META = {   # name: (style, reference)
    "LD PD FOLD": (HTK, "S64"), "LD PD SCREAM": (IND, "S60"), "LD 303 TRNC": (HTR, "S66"), "LD MOOG DIRT": (HT, "S64"),
    "LD MOOG ACID": (ACI, "S66"), "LD MENTASM": (RAV, "S63"), "LD HOOV DARK": (TE, "S63"), "LD RAVE HV": (RAV, "S63"),
    "BS RUMBLE": (HT, "S40"), "BS SUB CLEAN": (TE, "S45"), "BS REESE DRK": (HT, "S41"), "BS PD FOLD": (HT, "S47"),
    "BS FM DIRTY": (HT, "S46"), "BS 303": (ACI, "S42"), "BS 303 RESO": (ACI, "S42"), "BS 303 DARK": (ACI, "S42"),
    "BS 303 DIST": (HT, "S42"), "BS 303 SUB": (ACI, "S42"), "BS 303 DUB": (DUB, "S42"), "BS MOOG": (TE, "S50"),
    "BS MOOG HARD": (HT, "S47"), "BS MOOG SUB": (TE, "S45"), "BS MOOG PLCK": (TE, "S44"), "BS MG REESE": (HT, "S41"),
    "BS HOOVER": (HTK, "S51"), "PA DARK": (TE, "S81"), "PA TRIO WIDE": (HTR, "S80"), "PA FM COLD": (TE, "S81"),
    "PA FM DARK": (IND, "S81"), "PA SS WIDE": (HTR, "S80"), "PA SS DARK": (TE, "S81"), "DR DARK": (IND, "S82"),
    "DR GRAIN": (IND, "S82"), "DR METAL": (IND, "S83"), "DR ATMO": (TE, "S82"), "DR RESO": (DUB, "S82"),
    "DR IRON BOW": (IND, "S83"), "DR BUZZ": (IND, "S82"), "DR MACHINE": (IND, "S83"), "ST RAVE": (RAV, "S22"),
    "ST SSAW": (HTR, "S22"), "ST FM IRON": (IND, "S29"), "ST SS STAB": (HTR, "S22"), "ST HOOVER": (RAV, "S24"),
    "ST INDUS": (IND, "S29"), "ST INDUS RES": (IND, "S29"), "FX RISER": (TE, "S90"), "FX DOWNLIFT": (TE, "S91"),
    "FX IMPACT": (TE, "S94"), "FX ZAP": (DET, "S96"), "FX TOLE": (IND, "S98"), "FX ANVIL": (IND, "S98"),
    "FX METAL HIT": (IND, "S98"), "FX SHEET": (IND, "S98"), "FX SLAM": (IND, "S94"), "FX SIREN": (RAV, "S97"),
    "FX LARSEN": (IND, "S98"), "FX LARSEN RES": (IND, "S98"), "FX RISER IND": (IND, "S90"), "KT HARD": (HT, "S110"),
    "KT HARDTECH": (HTK, "S110"), "KT INDUS": (IND, "S117"),
}
KEPT = []


def _kept():
    path = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))),
                        "livraison", "old", "PRESETS-phase3.md")
    if not os.path.exists(path):
        return
    gone = {(p.eng, p.replace) for p in BANK if p.replace}
    news = {(p.eng, p.name) for p in BANK}
    seen = set()
    for line in open(path):
        m = re.match(r"^\| ([A-Z][A-Z0-9 +&#./-]+?) \| ([A-Z0-9-]+) \| ([A-Z]+) \| [A-Z]+ \| (.*?) \| (.*?) \|$", line)
        if m and m.group(1)[:2] in ("LD", "BS", "PA", "ST", "DR", "FX", "KT", "PL", "PC"):
            eng, name = m.group(2), m.group(1)
            if (eng, name) in gone or (eng, name) in news or (eng, name) in seen:
                continue
            seen.add((eng, name))
            st, rf = KEPT_META.get(name, ("", ""))
            KEPT.append(K(eng, name, m.group(3), m.group(4), m.group(5), st, rf))
        m = re.match(r"^\| ([A-Z][A-Z0-9 +&#./-]+?) \| ([A-Z0-9-]+) \| [A-Z]+ \| (.*?) \|$", line)   # (KITS: 4 columns)
        if m and m.group(1)[:2] == "KT" and (m.group(2), m.group(1)) not in seen:
            seen.add((m.group(2), m.group(1)))
            st, rf = KEPT_META.get(m.group(1), ("", ""))
            KEPT.append(K(m.group(2), m.group(1), "KIT", m.group(3), "beat", st, rf))


_kept()
