/* SPDX-License-Identifier: GPL-3.0-only */
/* (techno-engines) Every knob of the techno engines does something: for SUPERSAW, ACID, MINI, HOOVER and INDUS, each
 * preset, each value of HOME, EDIT 1 and EDIT 2 (and MINI's OSC 1, OSC 2, OSC 3, MINI MOD) swept from its minimum to
 * its maximum in regular steps (every value of a short list), a held note rendered through the firmware's own engine,
 * voices and mix (tests/hostsim.c; the sends at 0: the dry sound), ACID a 303 line on the sequencer (accents, slides).
 * Measured each time: the level and four bands (under 250 Hz, 250 Hz .. 1.5 kHz, 1.5 .. 6 kHz, over 6 kHz) in four
 * windows (the attack, the held note, the release's start, its tail). A step's change is the largest difference of
 * those (dB). Verdicts, per preset and knob:
 *   OK         the sweep moves the sound (3 dB or more somewhere), smoothly
 *   NO EFFECT  under 1 dB over the whole range (in this preset: some knobs only act with another set, a DETUNE with
 *              its oscillator silent); FAIL if it is so in every preset of its engine
 *   DEAD x..y  more than a third of the range moves the sound by under 0.3 dB a step (a warning)
 *   JUMP at v  one step moves the level 4 times more than the average step and by 12 dB or more (a continuous knob,
 *              not its first step out of 0: FAIL); STEP at v: one band only (a warning: a filter envelope reaching a
 *              note it did not before)
 *   CLIP at v  the output at full scale for more than 1 % of the samples (FAIL)
 *   SILENT at v the held note under -70 dBFS where the preset's own value sounds (a warning: LEVEL 0, a filter shut)
 * Then CC74 and CC71 on each engine: the parameter they set (the engine's CUTOFF / RES) and that their sweep moves the
 * sound. A third argument: the report as a markdown table (livraison/KNOBS-CHECK.md).
 *   cc -O2 -w -Ibuild/gen -Ifirmware/src -o build/host/knobs_test tests/knobs_test.c -lm
 *   build/host/knobs_test [REPORT.md] */
#define main hostsim_main
#include "hostsim.c"
#undef main

#define NSEG 4
#define NFEAT (NSEG * 6)
#define NSTEPS 9
typedef struct { double b0, b1, b2, a1, a2, x1, x2, y1, y2; } bq_t;
static void bq_set(bq_t *q, int kind, double f, double qq)   /* RBJ: 0 LP, 1 HP, 2 BP */
{
    double w = 2 * M_PI * f / FS, c = cos(w), s = sin(w), al = s / (2 * qq), a0 = 1 + al;
    memset(q, 0, sizeof *q);
    if (kind == 0) { q->b0 = (1 - c) / 2; q->b1 = 1 - c; q->b2 = (1 - c) / 2; }
    else if (kind == 1) { q->b0 = (1 + c) / 2; q->b1 = -(1 + c); q->b2 = (1 + c) / 2; }
    else { q->b0 = al; q->b1 = 0; q->b2 = -al; }
    q->b0 /= a0; q->b1 /= a0; q->b2 /= a0; q->a1 = -2 * c / a0; q->a2 = (1 - al) / a0;
}
static double bq_run(bq_t *q, double x)
{
    double y = q->b0 * x + q->b1 * q->x1 + q->b2 * q->x2 - q->a1 * q->y1 - q->a2 * q->y2;
    q->x2 = q->x1; q->x1 = x; q->y2 = q->y1; q->y1 = y;
    return y;
}
static double db(double e, double n) { return n > 0 && e > 0 ? fmax(-80.0, 10 * log10(e / n)) : -80.0; }

static int seq_mode;                       /* ACID: the line on the sequencer */
static const struct { uint8_t n, acc, sl; } LINE[16] = {
    {33, 1, 0}, {33, 0, 0}, {45, 0, 1}, {33, 0, 0}, {36, 1, 0}, {0, 0, 0}, {43, 0, 1}, {45, 1, 0},
    {33, 0, 0}, {40, 0, 1}, {33, 1, 0}, {45, 0, 0}, {31, 0, 1}, {33, 1, 1}, {45, 0, 0}, {48, 1, 0}};
typedef struct { double f[NFEAT]; double clip, sus_db; } meas_t;

/* engine e preset pi with parameter id at v (id P_COUNT: none), note: features into *m */
static void measure(uint32_t e, uint32_t pi, uint32_t id, int32_t v, uint32_t note, meas_t *m, int cc, int ccv)
{
    track_t *t = &trk[0];
    int32_t o[2 * CTL];
    uint32_t f, i, k, total, off, seg[NSEG + 1];
    double en[NSEG][5], cnt[NSEG], zc[NSEG], prev = 0;
    bq_t q[5];
    uint32_t nclip = 0;
    host_tracks_init();
    host_preset(t, e, pi);
    t->p[P_AMODE] = 0;
    t->p[P_DIST] = t->p[P_CHOR] = t->p[P_DLY] = t->p[P_REV] = 0;
#ifdef ENGI_MINI
    mini_poll();                           /* (MINI: as the main loop, before and after the knob: SLOT loads its patch) */
#endif
    if (id < P_COUNT)
        t->p[id] = (int16_t)v;
#ifdef ENGI_MINI
    mini_poll();
#endif
    if (cc)
        midi_cc_map(t, (uint32_t)cc, (uint32_t)ccv);
    bq_set(&q[0], 0, 250, 0.707);
    bq_set(&q[1], 2, 612, 0.8);
    bq_set(&q[2], 2, 3000, 0.8);
    bq_set(&q[3], 1, 6000, 0.707);
    bq_set(&q[4], 0, 900, 0.707);
    memset(zc, 0, sizeof zc);
    memset(en, 0, sizeof en);
    memset(cnt, 0, sizeof cnt);
    if (seq_mode) {                        /* the line once, 145 BPM 1/16, GATE 50 % */
        uint32_t s;
        for (s = 0; s < 16u; s++) {
            uint8_t nt = LINE[s].n;
            put_step(t, s, nt ? 1u : 0u, &nt, nt ? ST_NOTE : ST_REST,
                     (LINE[s].acc ? SF_ACCENT : 0u) | (LINE[s].sl ? SF_SLIDE : 0u));
        }
        t->p[P_SLEN] = 16;
        t->p[P_SDIV] = 2;
        t->p[P_SGATE] = 64;
        song.g[G_BPM] = 145;
        transport_req = 1;
        total = (uint32_t)(FS * 60.0 / 145.0 / 4.0 * 16) + FS / 2;
        off = (uint32_t)(FS * 60.0 / 145.0 / 4.0 * 16);
        for (k = 0; k <= NSEG; k++)
            seg[k] = off * k / NSEG;
        seg[NSEG] = total;
    } else {                               /* a note, a second one a fifth up over it at 0.3 s (legato: a glide), both off at 0.6 s */
        input_on(t, note, 100);
        total = FS;
        off = FS * 6 / 10;
        seg[0] = FS / 50; seg[1] = FS * 15 / 100; seg[2] = off; seg[3] = FS * 75 / 100; seg[4] = total;
    }
    for (f = 0; f < total; f += CTL) {
        if (!seq_mode && f >= FS * 3 / 10 && f < FS * 3 / 10 + CTL)
            input_on(t, note + 7, 100);
        if (!seq_mode && f >= off && f < off + CTL) {
            input_off(t, note);
            input_off(t, note + 7);
        }
        if (seq_mode && f >= off && song.playing)
            transport_req = 2;
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++) {
            double x = (o[2 * i] + o[2 * i + 1]) * 0.5 / 32768.0;
            uint32_t n = f + i, s;
            if (abs(o[2 * i]) >= 32700 || abs(o[2 * i + 1]) >= 32700)
                nclip++;
            for (s = 0; s < NSEG && !(n >= seg[s] && n < seg[s + 1]); s++)
                ;
            {
                double b[4], z = bq_run(&q[4], x);
                for (k = 0; k < 4u; k++)
                    b[k] = bq_run(&q[k], x);
                if (s < NSEG && ((z >= 0) != (prev >= 0)) && fabs(z) > 1e-4)
                    zc[s]++;
                prev = z;
                if (s < NSEG) {
                    en[s][0] += x * x;
                    for (k = 0; k < 4u; k++)
                        en[s][k + 1] += b[k] * b[k];
                    cnt[s]++;
                }
            }
        }
    }
    for (k = 0; k < NSEG; k++) {
        for (i = 0; i < 5u; i++)
            m->f[k * 6 + i] = db(en[k][i], cnt[k]);
        m->f[k * 6 + 5] = m->f[k * 6] > -50 && zc[k] > 4 ? 3 * 12 * log2(zc[k] * FS / cnt[k]) : -80;   /* (the pitch:
                                                         * 3 a semitone, so that a 1/3 semitone counts as 1 dB) */
    }
    m->clip = (double)nclip / total;
    m->sus_db = m->f[6];
}
static int use_pitch;                      /* the knob moves the pitch (TUNE, SLIDE, GLIDE, DETUNE, RANGE, DIVE, ..): its
                                            * zero crossings count; else not (a filter moves them too) */
static int pitch_knob(const char *l)
{
    static const char *const K[] = {"TUNE", "SLIDE", "GLIDE", "DETUNE", "DETUNE 2", "DETUNE 3", "RANGE 1", "RANGE 2",
                                    "RANGE 3", "DIVE", "D.TIME", "RATIO", "P.ENV", "P.DECAY", "SUB"};
    uint32_t i;
    for (i = 0; i < NELEM(K); i++)
        if (!strcmp(l, K[i]))
            return 1;
    return 0;
}
static double dist(const meas_t *a, const meas_t *b)
{
    double d = 0;
    uint32_t i;
    for (i = 0; i < NFEAT; i++) {           /* (under -60 dB: inaudible here, as -60) */
        double x, fa = a->f[i], fb = b->f[i];
        if (i % 6u == 5u) {                  /* the pitch: only where both sound */
            x = use_pitch && fa > -80 && fb > -80 ? fabs(fa - fb) : 0;
        } else
            x = fabs((fa > -60 ? fa : -60) - (fb > -60 ? fb : -60));
        d = x > d ? x : d;
    }
    return d;
}

static FILE *rep;
static int fails;
typedef struct { uint32_t id; const char *page; } knob_t;

/* the knobs of engine e: HOME, EDIT 1, EDIT 2 (and MINI's pages), each once */
static uint32_t knobs_of(uint32_t e, knob_t *k)
{
    uint32_t n = 0, i, j;
    const engine_t *en = ENGINES[e];
    for (i = 0; i < 4u; i++) {
        k[n].id = en->knob[i];
        k[n++].page = "HOME";
    }
    for (i = 0; i < 8u; i++) {
        for (j = 0; j < n && k[j].id != P_E0 + i; j++)
            ;
        if (j < n) {
            k[j].page = i < 4u ? "HOME, EDIT 1" : "HOME, EDIT 2";
            continue;
        }
        k[n].id = P_E0 + i;
        k[n++].page = i < 4u ? "EDIT 1" : "EDIT 2";
    }
#ifdef ENGI_MINI
    if (e == ENGI_MINI)
        for (i = P_MN_R1; i <= P_MN_FMODE; i++) {
            k[n].id = i;
            k[n++].page = i < P_MN_R2 ? "OSC 1" : i < P_MN_R3 ? "OSC 2" : i < P_MN_KBD ? "OSC 3" : "MINI MOD";
        }
#endif
    return n;
}

/* a knob without effect in preset pi because another value leaves it nothing to act on */
static const char *why_idle(uint32_t e, uint32_t pi, uint32_t id)
{
    const int16_t *p;
    host_tracks_init();
    host_preset(&trk[0], e, pi);
#ifdef ENGI_MINI
    mini_poll();
#endif
    p = trk[0].p;
    (void)p;
#ifdef ENGI_MINI
    if (e == ENGI_MINI) {
        static const uint8_t L[3] = {P_MN_L1, P_MN_L2, P_MN_L3};
        uint32_t k = id >= P_MN_R3 && id <= P_MN_L3 ? 2u : id >= P_MN_R2 && id <= P_MN_L2 ? 1u : id <= P_MN_W1 && id >= P_MN_R1 ? 0u : 3u;
        if (k < 3u && !p[L[k]])
            return " (inactive here: that oscillator's LEVEL is 0)";
        if (id == P_MN_NCOL && !p[P_MN_NZ])
            return " (inactive here: NOISE at 0)";
        if (id == P_MN_MDST && !p[P_E6])
            return " (inactive here: MOD at 0)";
        if (id == P_E6 && !p[P_MN_MDST])
            return " (inactive here: MOD TO OFF)";
        if (id == P_GLIDE)
            return " (a glide between notes: see the legato note)";
    }
#endif
#ifdef ENGI_INDUS
    if (e == ENGI_INDUS && id == P_E5 + 1u && !p[P_E5])
        return " (inactive here: P.ENV at 0)";
#endif
    return "";
}

static void sweep_engine(uint32_t e)
{
    knob_t kn[32];
    uint32_t nk = knobs_of(e, kn), ki, pi;
    const engine_t *en = ENGINES[e];
    seq_mode = e == ENGI_ACID;
    for (ki = 0; ki < nk; ki++) {
        uint32_t id = kn[ki].id, any = 0;
        const param_desc_t *d = param_desc_of(e, id);
        int is_enum = d->fmt == F_ENUM || d->fmt == F_ONOFF;
        use_pitch = pitch_knob(d->label);
        int32_t nv = is_enum ? d->max - d->min + 1 : NSTEPS;
        for (pi = 0; pi < en->npresets; pi++) {
            const preset_t *pr = &en->presets[pi];
            uint32_t note = pr->tag == PT_BASS ? 45u : 57u, s;
            meas_t m[16], base;
            int32_t vals[16];
            double tot = 0, mx = 0, sum = 0, dead = 0;
            char verdict[96] = "OK";
            int32_t jump_v = -999, clip_v = -999, sil_v = -999, jump_lvl = 0;
            measure(e, pi, P_COUNT, 0, note, &base, 0, 0);
            for (s = 0; s < (uint32_t)nv && s < 16u; s++) {
                vals[s] = is_enum ? d->min + (int32_t)s : d->min + (int32_t)((d->max - d->min) * (int64_t)s / (NSTEPS - 1));
                measure(e, pi, id, vals[s], note, &m[s], 0, 0);
                if (m[s].clip > 0.01 && clip_v == -999)
                    clip_v = vals[s];
                if (m[s].sus_db < -70 && base.sus_db > -60 && sil_v == -999)
                    sil_v = vals[s];
            }
            if (getenv("KDBG") && !strcmp(getenv("KDBG"), d->label) && pi == (uint32_t)atoi(getenv("KPI") ? getenv("KPI") : "0"))
                for (s = 0; s < (uint32_t)nv && s < 16u; s++) {
                    uint32_t j;
                    printf("    v %4d:", vals[s]);
                    for (j = 0; j < NFEAT; j++)
                        printf(" %5.1f%s", m[s].f[j], j % 6u == 5u ? " |" : "");
                    printf("\n");
                }
            for (s = 1; s < (uint32_t)nv && s < 16u; s++) {
                double x = dist(&m[s], &m[s - 1]);
                sum += x;
                mx = x > mx ? x : mx;
                if (x < 0.3)
                    dead++;
            }
            for (s = 1; s < (uint32_t)nv && s < 16u; s++) {
                double x = dist(&m[s], &m[0]);
                tot = x > tot ? x : tot;
            }
            for (s = 2; !is_enum && s < (uint32_t)nv && s < 16u; s++) {   /* (not out of 0: a level, a send coming in) */
                double x = dist(&m[s], &m[s - 1]), avg = (sum - x) / (nv - 2);
                if (x >= 12.0 && x > 4 * avg && jump_v == -999) {
                    uint32_t j;
                    jump_v = vals[s];
                    jump_lvl = 0;
                    for (j = 0; j < NFEAT; j += 6)   /* the level itself (not a band) moved that much: a fault */
                        jump_lvl |= fabs(fmax(m[s].f[j], -60) - fmax(m[s - 1].f[j], -60)) >= 12.0;
                }
            }
            if (tot < 1.0)
                snprintf(verdict, sizeof verdict, "NO EFFECT%s", why_idle(e, pi, id));
            else {
                any = 1;
                if (!is_enum && dead > (nv - 1) / 3.0)
                    snprintf(verdict, sizeof verdict, "DEAD %.0f %% of the range", 100.0 * dead / (nv - 1));
                if (jump_v != -999) {
                    snprintf(verdict, sizeof verdict, jump_lvl ? "JUMP at %d" : "STEP at %d (one band)", jump_v);
                    fails += jump_lvl;
                }
            }
            if (clip_v != -999) {
                snprintf(verdict + strlen(verdict), sizeof verdict - strlen(verdict), ", CLIP at %d", clip_v);
                fails++;
            }
            if (sil_v != -999)
                snprintf(verdict + strlen(verdict), sizeof verdict - strlen(verdict), ", SILENT at %d", sil_v);
            if (strcmp(verdict, "OK"))
                printf("  %-8s %-13s %-9s %-12s %s (swing %.1f dB, largest step %.1f dB)\n", en->name, pr->name,
                       d->label, kn[ki].page, verdict, tot, mx);
            if (rep)
                fprintf(rep, "| %s | %s | %s | %s | %.1f | %.1f | %s |\n", en->name, pr->name, d->label, kn[ki].page,
                        tot, mx, verdict);
        }
        if (!any) {
            printf("FAIL %-8s %s: no effect in any preset\n", en->name, d->label);
            fails++;
        }
    }
}

static void cc_check(uint32_t e)
{
    static const int CC[2] = {74, 71};
    uint32_t c, pi = 0;
    for (c = 0; c < 2u; c++) {
        track_t *t = &trk[0];
        int32_t before;
        uint32_t id = P_COUNT, i;
        meas_t lo, hi;
        host_tracks_init();
        host_preset(t, e, pi);
        for (i = 0; i < P_COUNT; i++)
            ;
        before = t->p[P_E0];
        {
            int16_t snap[P_COUNT];
            memcpy(snap, t->p, sizeof snap);
            midi_cc_map(t, (uint32_t)CC[c], 0);
            for (i = 0; i < P_COUNT; i++)
                if (t->p[i] != snap[i])
                    id = i;
            if (id == P_COUNT) {
                memcpy(t->p, snap, sizeof snap);
                midi_cc_map(t, (uint32_t)CC[c], 127);
                for (i = 0; i < P_COUNT; i++)
                    if (t->p[i] != snap[i])
                        id = i;
            }
        }
        (void)before;
        seq_mode = e == ENGI_ACID;
        use_pitch = 0;
        measure(e, pi, P_COUNT, 0, 45, &lo, CC[c], 0);
        measure(e, pi, P_COUNT, 0, 45, &hi, CC[c], 127);
        {
            const char *lab = id < P_COUNT ? param_desc_of(e, id)->label : "(none)";
            double d = dist(&lo, &hi);
            printf("  CC%d on %-8s -> %-8s swing %.1f dB%s\n", CC[c], ENGINES[e]->name, lab, d,
                   id < P_COUNT && d < 1.0 ? "  NO EFFECT" : "");
            if (rep)
                fprintf(rep, "| %s | CC%d | %s | | %.1f | | %s |\n", ENGINES[e]->name, CC[c], lab, d,
                        id == P_COUNT ? "none (as this engine has none)" : d < 1.0 ? "NO EFFECT" : "OK");
            if (id < P_COUNT && d < 1.0)
                fails++;
        }
    }
}

int main(int argc, char **argv)
{
    static const char *const ENG[] = {"SUPERSAW", "ACID", "MINI", "HOOVER", "INDUS"};
    uint32_t i, e;
    if (argc > 1 && !(rep = fopen(argv[1], "w"))) {
        perror(argv[1]);
        return 2;
    }
    if (rep)
        fprintf(rep, "| Moteur | Preset | Knob | Page | Amplitude (dB) | Plus grand pas (dB) | Verdict |\n|---|---|---|---|---|---|---|\n");
    for (i = 0; i < NELEM(ENG); i++) {
        for (e = 0; e < NENGINES && strcmp(ENGINES[e]->name, ENG[i]); e++)
            ;
        if (e == NENGINES || !eng_ok(e))
            continue;
        printf("%s\n", ENG[i]);
        sweep_engine(e);
        cc_check(e);
    }
    if (rep)
        fclose(rep);
    printf("%s: %d failure(s)\n", fails ? "FAIL" : "ok", fails);
    return fails ? 1 : 0;
}
