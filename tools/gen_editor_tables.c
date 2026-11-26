/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments; Gir Editor (2026) */
/* Gir Editor's tables, from the firmware's own sources (through tests/hostsim.c, as tests/descdump.c): the pages of
 * the device (params.c PAGES, in its order, each with its family, its four parameter ids and on which engines it is
 * shown: ui.c page_visible's rules), the track and global parameters (TP, GP), every engine (its EDIT pages' titles
 * and descriptors, HOME's knobs, its voices, its presets with their category), the engines' order on the EDIT layer,
 * the MIDI CC map (midi_control.c), and the ids the editor names. A Gir 1.1 that adds a page or a parameter changes
 * this output, and so the editor, with no edit to the editor.
 *   tools/gen_editor_tables.sh    (builds this, writes web/gir-editor/src/generated/tables.json) */
#define FELUCCA_LEGACY 1
#define main hostsim_main
#include "../tests/hostsim.c"
#undef main
#ifndef FELUCCA_VERSION
#define FELUCCA_VERSION "?"
#endif

static void js_str(const char *s)
{
    putchar('"');
    for (; s && *s; s++) {
        if (*s == '"' || *s == '\\')
            putchar('\\');
        putchar(*s);
    }
    putchar('"');
}

static void js_desc(const param_desc_t *d)
{
    int32_t i;
    printf("{\"label\":");
    js_str(d->label);
    printf(",\"fmt\":%d,\"min\":%d,\"max\":%d,\"def\":%d,\"names\":", d->fmt, d->min, d->max, d->def);
    if (d->fmt == F_ENUM && d->names) {
        putchar('[');
        for (i = 0; i <= d->max - d->min; i++) {
            if (i)
                putchar(',');
            js_str(d->names[i]);
        }
        putchar(']');
    } else {
        printf("null");
    }
    printf(",\"unit\":");
    js_str(d->unit ? PD_UNIT_OF(d) : "");
    putchar('}');
}

static void js_descs(const param_desc_t *d, uint32_t n)
{
    uint32_t i;
    putchar('[');
    for (i = 0; i < n; i++) {
        if (i)
            printf(",\n  ");
        js_desc(&d[i]);
    }
    putchar(']');
}

static const char *const FAM_NAME[FAM_COUNT] = {"HOME", "ENV", "LFO", "FX", "SCL", "EDIT", "GLO", "SAVE", "ARP", "SEQ",
                                                 "TRK"};
static const char *const SCOPE_NAME[] = {"track", "global", "engine", "step", "tracks"};

/* ui.c page_visible, for track engine e (the device asks it of the selected track; this asks it of each engine).
 * GR_SMIX (SAMPLE on a USR set with samples) depends on the samples: "sample-usr" */
static int page_on(uint32_t i, uint32_t e, const char **cond)
{
    const engine_t *en = ENGINES[e];
    uint32_t id = PAGES[i].id[0], g = PAGES[i].graph;
    *cond = 0;
    if (g == GR_FMOP || g == GR_FMALL)                 /* (ui_fm6.c sheet_of: FM6's sheets) */
        return e == ENGI_FM6;
    if (g == GR_DXSND)                                 /* (ui_dx.c: DRUM-X's sheet) */
        return e == ENGI_DRUMX;
    if (g == GR_LANES)
        return e == ENGI_DRUM;
    if (g == GR_LMIX)
        return drum_engine(e);
    if (g == GR_SMIX) {
        *cond = "sample-usr";
        return e == ENGI_SAMPLE;
    }
    if (en->oneshot && (PAGES[i].fam == FAM_SCL || id == P_ATK || id == P_EG2_ATK || id == P_VOICE || id == P_ALLOC))
        return 0;
    if (en->ownenv && (id == P_ATK || id == P_EG2_ATK))
        return 0;
    if (g == GR_SLICES)
        return FELUCCA_SLICE && e == ENGI_SLICE;
    if (PAGES[i].fam == FAM_EDIT && (id == P_RB_MIX || id == P_RB_TUNE || id == P_KCLICK))
        return drum_engine(e);
    if (PAGES[i].fam == FAM_EDIT && id >= P_MN_R1 && id <= P_MN_FMODE)
        return e == ENGI_MINI;
    return !(PAGES[i].fam == FAM_EDIT && PAGES[i].id[0] >= P_FM1_ATK && PAGES[i].id[0] <= P_FM4_LEVEL) ||
           (FELUCCA_FM4 && e == ENGI_DIGITAL);
}

/* a preset's category: its name's first word when it is one of the bank's prefixes (PRESETS.md), else "" */
static void js_cat(const char *name)
{
    static const char *const CAT[] = {"LD", "BS", "PA", "PL", "ST", "DR", "FX", "PC", "KT"};
    uint32_t k, n = 0;
    while (name[n] && name[n] != ' ')
        n++;
    for (k = 0; k < NELEM(CAT); k++)
        if (n == strlen(CAT[k]) && !strncmp(name, CAT[k], n)) {
            js_str(CAT[k]);
            return;
        }
    js_str("");
}

#define ID(x) printf("%s\"" #x "\":%d", first ? "" : ",", (int)(x)), first = 0

int main(void)
{
    uint32_t e, i, k, first = 1;
    printf("{\"generator\":\"tools/gen_editor_tables.c\",\"version\":");
    js_str(FELUCCA_VERSION);
    printf(",\n\"P_COUNT\":%d,\"G_COUNT\":%d,\"NSTEP\":%d,\"P_E0\":%d,\"NTRK\":%d,\"NENGINES\":%d,\n",
           P_COUNT, G_COUNT, NSTEP, P_E0, NTRK, NENGINES);
    printf("\"ID\":{");
    ID(P_LEVEL); ID(P_PAN); ID(P_MUTE); ID(P_ATK); ID(P_DEC); ID(P_SUS); ID(P_REL);
    ID(P_EG2_ATK); ID(P_EG2_DEC); ID(P_EG2_SUS); ID(P_EG2_REL);
    ID(P_LRATE); ID(P_LWAVE); ID(P_LPHASE); ID(P_LFADE);
    ID(P_DIST); ID(P_CHOR); ID(P_DLY); ID(P_REV); ID(P_DTYPE); ID(P_DTONE);
    ID(P_SLEN); ID(P_SDIV); ID(P_SSWING); ID(P_SGATE); ID(P_DIR);
    ID(P_DKSRC); ID(P_DKAMT); ID(P_DKREL);
    ID(P_EQ_LOW); ID(P_EQ_MID); ID(P_EQ_HIGH); ID(P_DEPTH);
    ID(P_RB_MIX); ID(P_RB_TUNE); ID(P_RB_DECAY); ID(P_RB_DRIVE); ID(P_RB_TONE); ID(P_RB_DUCK); ID(P_RB_ATK);
    ID(P_RB_SLICE); ID(P_KCLICK); ID(P_KPUNCH);
    ID(P_M1SRC); ID(P_LN0); ID(P_LPAN0); ID(P_LTUNE0); ID(P_LDECAY0); ID(P_E0);
    ID(G_BPM); ID(G_SWING); ID(G_ENGSEL); ID(G_DTIME); ID(G_DFDBK); ID(G_DCOLOR); ID(G_DMIX);
    ID(G_RTYPE); ID(G_RSIZE); ID(G_RDAMP); ID(G_CRATE); ID(G_CDEPTH); ID(G_CMODE); ID(G_CFDBK);
    printf("},\n\"FAM\":[");
    for (k = 0; k < FAM_COUNT; k++)
        printf("%s\"%s\"", k ? "," : "", FAM_NAME[k]);
    printf("],\n\"PAGES\":[");
    for (i = 0; i < NPAGES; i++) {
        const page_t *pg = &PAGES[i];
        const char *cond = 0;
        printf("%s\n {\"title\":", i ? "," : "");
        js_str(pg->title);
        printf(",\"fam\":\"%s\",\"scope\":\"%s\",\"graph\":%d,\"ids\":[", FAM_NAME[pg->fam], SCOPE_NAME[pg->scope], pg->graph);
        for (k = 0; k < 4u; k++)
            printf("%s%d", k ? "," : "", pg->id[k] == 0xFFu ? -1 : (int)pg->id[k]);
        printf("],\"slots\":[");                     /* each knob's scope: GLO's buses are global; (2.5) FX > DELAY and
                                                         * REVERB: KNOB 1 the track's SEND, 2..4 the bus's (page_desc) */
        for (k = 0; k < 4u; k++)
            printf("%s\"%s\"", k ? "," : "", pg->graph == GR_FXDLY || pg->graph == GR_FXREV ? (k ? "global" : "track")
                                                : SCOPE_NAME[pg->scope]);
        printf("],\"send\":%d,\"engines\":[", pg->graph == GR_FXDLY || pg->graph == GR_FXREV);
        for (e = 0, first = 1; e < NENGINES; e++)
            if (eng_ok(e) && page_on(i, e, &cond)) {
                printf("%s%u", first ? "" : ",", (unsigned)e);
                first = 0;
            }
        printf("]");
        (void)page_on(i, ENGI_SAMPLE, &cond);
        if (cond)
            printf(",\"cond\":\"%s\"", cond);
        printf("}");
    }
    printf("],\n\"TP\":");
    js_descs(TP, P_E0);
    printf(",\n\"GP\":");
    js_descs(GP, G_COUNT);
    printf(",\n\"ENG\":[");
    for (e = 0; e < NENGINES; e++) {
        const engine_t *en = ENGINES[e];
        printf("%s\n {\"index\":%u,\"name\":", e ? "," : "", (unsigned)e);
        js_str(eng_ok(e) ? en->name : "-");
        printf(",\"ok\":%d,\"titles\":[", eng_ok(e) ? 1 : 0);
        js_str(en->page_title[0]);
        putchar(',');
        js_str(en->page_title[1]);
        printf("],\"home\":[%d,%d,%d,%d],\"poly\":%d,\"oneshot\":%d,\"ownenv\":%d,\"cc74\":%d,\"cc71\":%d,\"edit\":",
               en->knob[0], en->knob[1], en->knob[2], en->knob[3], en->poly ? en->poly : NVOICE, en->oneshot, en->ownenv,
               (MIDI_CC_ENG[e] >> 4) - 1, (MIDI_CC_ENG[e] & 15) - 1);
        js_descs(en->edit, 8);
        printf(",\"presets\":[");
        for (k = 0; k < en->npresets; k++) {
            printf("%s\n  {\"name\":", k ? "," : "");
            js_str(en->presets[k].name);
            printf(",\"cat\":");
            js_cat(en->presets[k].name);
            printf("}");
        }
        printf("]}");
    }
    printf("],\n\"ORDER\":[");                          /* the EDIT layer's keys F3 .. G5, then INIT */
    for (k = 0; k < NENG_SHOWN; k++)
        printf("%s%u", k ? "," : "", (unsigned)eng_vis(k));
    printf("],\n\"CC\":[");
    for (k = 0; k < NELEM(MIDI_CC_MAP); k++)
        printf("%s[%d,%d]", k ? "," : "", MIDI_CC_MAP[k][0],
               MIDI_CC_MAP[k][1] == MCC_CUT ? -74 : MIDI_CC_MAP[k][1] == MCC_RES ? -71 : MIDI_CC_MAP[k][1]);
    printf("],\n\"TC_NAME\":[");
    for (k = 0; k < TC_COUNT; k++) {
        if (k)
            putchar(',');
        js_str(TC_NAME[k]);
    }
    printf("],\n\"LANE_NOTE\":[");
    for (k = 0; k < NLANE; k++)
        printf("%s%d", k ? "," : "", DRUM_LANE_NOTE[k]);
    printf("]}\n");
    return 0;
}
