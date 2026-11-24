/* SPDX-License-Identifier: GPL-3.0-only */
/* (techno-engines) SUPERSAW: the JP-8000's supersaw as a polyphonic engine, chords and all (ANALOG's UNISON is one note
 * on 8 voices). Written for Skiff after Adam Szabo's analysis of the JP-8000 ("How to Emulate the Super Saw", 2010):
 *   seven band-limited saws (dsp.c osc_saw: polyBLEP) around the note, their offsets the JP-8000's (SSAW_OFF: -11 % ..
 *   +10.7 % of the pitch at full DETUNE), DETUNE through its measured curve (SSAW_DT: barely moving up to half way,
 *   then wide); MIX the six around against the middle one by the measured gains (the middle one falls as they rise);
 *   new random phases for each note that starts from silence (a retriggered sounding voice keeps them: no click);
 *   a one-pole high-pass that follows the note (HPF: 64 = at the fundamental, as the JP-8000's: what the saws' beating
 *   leaves below the note goes, the low end stays clean), then DRIVE (ANALOG's soft clip) and ANALOG's low-pass (the
 *   trapezoidal SVF, CUTOFF RES KEYTRK); SUB a square an octave below, under the filter (weight for a lead).
 * The track's SHP routings (ENV / LFO DEST SHAPE, the matrix's SHP) move DETUNE. Stereo: the track's SPREAD (POLY's voices
 * left and right) and CHORUS, as ANALOG's. State per voice in the part's engine area (ssaw_vc_t: the 7 phases and the
 * sub's), voice_t.s[0..1] the low-pass, s[4] the high-pass. Six voices at most (engine_t.poly): see livraison/CPU.md. */
#define ENGI_SSAW 15u            /* engines.c ENGINES[] (append-only) */
#define SSAW_N 7
#define SSAW_POLY 6

typedef struct {
    uint32_t ph[SSAW_N];         /* the seven saws, the middle one ph[3] */
    uint32_t sub;                /* the sub square's phase (an octave below the middle saw) */
} ssaw_vc_t;

/* DETUNE 0..127 -> the JP-8000's detune amount, Q16 (Szabo's 11th-order fit of the measured curve, clipped to 0..1) */
static const uint16_t SSAW_DT[128] = {
    197, 458, 588, 640, 650, 645, 642, 653, 683, 735, 808, 899, 1006, 1125, 1252, 1384,
    1516, 1647, 1775, 1897, 2013, 2123, 2226, 2323, 2415, 2503, 2588, 2672, 2754, 2837, 2922, 3009,
    3099, 3192, 3289, 3391, 3496, 3604, 3715, 3829, 3945, 4061, 4178, 4295, 4411, 4525, 4636, 4745,
    4851, 4954, 5054, 5151, 5245, 5338, 5429, 5519, 5610, 5703, 5798, 5897, 6001, 6111, 6228, 6353,
    6488, 6633, 6789, 6957, 7137, 7329, 7534, 7752, 7983, 8226, 8480, 8747, 9024, 9311, 9608, 9914,
    10228, 10550, 10878, 11214, 11556, 11904, 12258, 12619, 12986, 13360, 13742, 14132, 14531, 14938, 15355, 15781,
    16217, 16662, 17115, 17574, 18038, 18503, 18968, 19427, 19877, 20313, 20729, 21123, 21488, 21822, 22125, 22398,
    22645, 22879, 23118, 23387, 23725, 24185, 24836, 25770, 27102, 28982, 31592, 35160, 39965, 46344, 54704, 65532};
/* each saw's pitch offset at full detune, Q16 of the note's frequency (the JP-8000's, measured) */
static const int16_t SSAW_OFF[SSAW_N] = {-7210, -4121, -1279, 0, 1305, 4074, 7042};

static ssaw_vc_t *ssaw_vc(const track_t *t, const voice_t *v)
{
    return &((ssaw_vc_t *)eng_part(t))[(uint32_t)(v - t->v) % NVOICE];
}

static void ssaw_note_on(track_t *t, voice_t *v)
{
    ssaw_vc_t *S = ssaw_vc(t, v);
    uint32_t k;
    if (!v->env_out) {                                /* from silence: new phases (the JP-8000's run free) */
        int32_t seed = (int32_t)(0x2545F491u ^ (v->age * 2654435761u));
        for (k = 0; k < SSAW_N; k++)
            S->ph[k] = noise32(&seed);
        S->sub = S->ph[SSAW_N / 2] >> 1;
        v->s[0] = v->s[1] = v->s[4] = 0;              /* the filters (keep: a retrigger keeps them) */
    }
}

static void ssaw_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    ssaw_vc_t *S = ssaw_vc(t, v);
    uint32_t inc[SSAW_N], ph[SSAW_N], sub = S->sub, isub, i, k;
    int32_t det = clamp(p[P_E0] + ((m->shape - (64 << 8)) >> 8), 0, 127), dt = SSAW_DT[det];
    int32_t mix = clamp(p[P_E1], 0, 127);
    int32_t gc = 32697 - 18142 * mix / 127;                                     /* Szabo: -0.55366 x + 0.99785 */
    int32_t gs = 1454 + (42077 * mix - 24171 * mix * mix / 127) / 127;          /* -0.73764 x^2 + 1.2841 x + 0.044372 */
    int32_t cut = (p[P_E2] << 8) + m->cutoff + (p[P_E6] * (v->pitch16 - 60 * 16) >> 4);
    int32_t hpk = (int32_t)(m->inc / 20861u) * p[P_E4] >> 6;                   /* Q15: 64 -> the fundamental */
    int32_t drv = p[P_E5], drive = 32768 + drv * 512, sl = p[P_E7] * 200;
    int32_t ic1 = v->s[0], ic2 = v->s[1], lp = v->s[4];
    tsvf_t flt;
    if (hpk > 16000)
        hpk = 16000;
    for (k = 0; k < SSAW_N; k++) {
        int32_t f = (SSAW_OFF[k] * dt) >> 16;                                   /* Q16 */
        inc[k] = m->inc + (uint32_t)(int32_t)(((int64_t)m->inc * f) >> 16);
        ph[k] = S->ph[k];
    }
    isub = m->inc >> 1;
    tsvf_coef(&flt, cut, p[P_E3]);
    for (i = 0; i < n; i++) {
        int32_t c = osc_saw(ph[3], inc[3]), sd = 0, s;
        for (k = 0; k < SSAW_N; k++)
            if (k != 3u)
                sd += osc_saw(ph[k], inc[k]);
        for (k = 0; k < SSAW_N; k++)
            ph[k] += inc[k];
        s = ((sd >> 3) * gs + (c >> 3) * gc) >> 14;                             /* ~ +-32k */
        lp += ((s - lp) * hpk) >> 15;                                           /* the high-pass that follows the note */
        s -= lp;
        if (sl) {
            s += mulq15(osc_pulse(sub, isub, 0x80000000u), sl) >> 1;
            sub += isub;
        }
        if (drv)
            s = softclip(((s >> 2) * (drive >> 2)) >> 11);
        s = soft_knee(tsvf_lp(&flt, s >> 1, &ic1, &ic2), 16000) << 1;
        out[i] += voice_amp2(s, m, i);
    }
    for (k = 0; k < SSAW_N; k++)
        S->ph[k] = ph[k];
    S->sub = sub;
    v->s[0] = ic1;
    v->s[1] = ic2;
    v->s[4] = lp;
}

static const preset_t SSAW_PRESETS[] = {
    /* name, {DETUNE, MIX, CUTOFF, RES, HPF, DRIVE, KEYTRK, SUB}, {A D S R}, fenv, mono, sends, more, type */
    {"LD HARD SAW", {80, 100, 118, 10, 64, 20, 64, 30}, {2, 72, 112, 58}, 6, 0, FX(10, 60, 45, 55), MORE({P_SPRD, 110}), TAG(PT_LEAD)},
    {"LD SS TRANCE", {64, 90, 108, 18, 64, 0, 72, 0}, {4, 74, 110, 62}, 10, 0, FX(0, 70, 55, 65), MORE({P_SPRD, 110}), TAG(PT_LEAD)},
    {"LD SS ANTHEM", {96, 115, 116, 10, 64, 10, 64, 40}, {2, 76, 115, 64}, 4, 0, FX(5, 90, 60, 70), MORE({P_SPRD, 127}), TAG(PT_LEAD)},
    {"LD SS HTECH", {76, 100, 104, 22, 64, 70, 64, 50}, {0, 70, 115, 40}, 12, 0, FX(60, 40, 35, 30), MORE({P_DTYPE, 1}, {P_SPRD, 100}), TAG(PT_LEAD)},
    {"LD SS PLUCK", {60, 90, 50, 35, 64, 0, 64, 0}, {0, 70, 0, 56}, 50, 0, FX(0, 60, 70, 50), MORE({P_SPRD, 100}), TAG(PT_PLUCK)},
    {"ST SS STAB", {72, 100, 96, 25, 64, 15, 64, 0}, {0, 68, 0, 52}, 30, 0, FX(10, 80, 55, 60), MORE({P_SPRD, 120}), TAG(PT_KEYS)},
    {"PA SS WIDE", {90, 110, 85, 8, 70, 0, 50, 0}, {80, 90, 118, 98}, 4, 0, FX(0, 110, 30, 105), MORE({P_SPRD, 127}, {P_LRATE, 30}, {P_LD_FLT, 8}), TAG(PT_PAD)},
    {"PA SS DARK", {84, 100, 52, 20, 80, 0, 40, 0}, {60, 90, 118, 96}, 12, 0, FX(0, 90, 25, 110), MORE({P_SPRD, 127}, {P_LRATE, 22}, {P_LD_FLT, 14}), TAG(PT_PAD)},
    {"BS SUPERSAW", {40, 70, 60, 30, 32, 30, 64, 90}, {0, 64, 90, 22}, 30, 1, FX(30, 0, 10, 5), TAG(PT_BASS)},
};

static const engine_t ENG_SSAW = {
    .name = "SUPERSAW",
    .page_title = {"SAW", "FLT"},
    .edit = {
        {"DETUNE", 0, 0, 127, 64, F_INT, 0},
        {"MIX", 0, 0, 127, 90, F_PCT, 0},
        {"CUTOFF", 0, 0, 127, 110, F_CUTOFF, 0},
        {"RES", 0, 0, 127, 10, F_PCT, 0},
        {"HPF", 0, 0, 127, 64, F_PCT, 0},
        {"DRIVE", 0, 0, 127, 0, F_PCT, 0},
        {"KEYTRK", 0, 0, 127, 64, F_PCT, 0},
        {"SUB", 0, 0, 127, 0, F_PCT, 0},
    },
    .presets = SSAW_PRESETS,
    .npresets = NELEM(SSAW_PRESETS),
    .note_on = ssaw_note_on,
    .render = ssaw_render,
    .knob = {P_E0, P_E1, P_E2, P_REL},
    .poly = SSAW_POLY,
    .keep = 0x13,                /* the low-pass (s[0..1]), the high-pass (s[4]) */
};
