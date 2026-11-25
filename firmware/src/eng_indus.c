/* SPDX-License-Identifier: GPL-3.0-only */
/* (techno-engines) INDUS: industrial textures, metal and machines: a light engine. Written for Skiff.
 *   two sines, A at the note and B at RATIO (0.25 .. 8 times the note, on a continuous scale: inharmonic between the
 *   whole ratios, -24 .. +36 semitones, 64 ~ x 1.42); MOD: B modulates A's phase (cross / FM, up to ~4 radians: bells,
 *   sheet metal); RING: A x B against A (ring modulation: sirens, robots); NOISE: white noise under it; FOLD: a sine
 *   wavefolder (1x .. ~4x: grit, harsh harmonics); P.ENV / P.DECAY: every struck note starts P.ENV above the note (0 ..
 *   48 semitones) and falls to it in P.DECAY (an exponential, ANALOG's time law): impacts, zaps, machine hits; CUTOFF: a
 *   one-pole low-pass (127: open), RES its resonance: a peak at the cutoff (ANALOG's SVF, its band-pass at that
 *   frequency, added to the low-pass: at RES 0 the low-pass alone, as INDUS was; up: whistles, tuned feedback).
 * HOME: RATIO MOD FOLD CUTOFF. CUTOFF is a common parameter (core.h P_IN_CUT: HOME's KNOB 4, CC74), RES is E7 (EDIT 2,
 * CC71), where TONE was: a sound from before (P_IN_CUT -1) plays its E7 as the CUTOFF with RES 0, and indus_poll moves it
 * there (the main loop). The track's SHP routings move MOD (the matrix, ENV / LFO DEST SHAPE: a sweeping bell), its FLT
 * routings CUTOFF. Six voices at most (engine_t.poly: see livraison/CPU.md); per voice: v->ph[0] A, v->ph[1] B, s[0] the
 * pitch envelope (1/16 semitone << 8), s[1] the low-pass, s[2] the noise, s[3] s[4] the resonance's SVF. */
/* ENGI_INDUS 19 (core.h; engines.c ENGINES[], append-only) */
#define INDUS_POLY 6
/* CUTOFF 0..127 -> the one-pole's -3 dB frequency on ANALOG's cutoff scale (30 Hz .. 16 kHz, << 8): the resonance's
 * SVF at the same frequency (computed from the one-pole's k = 600 + c^2 x 2, Q15) */
static const uint16_t INDUS_SVF_CUT[128] = {
    7581, 7598, 7650, 7735, 7852, 7999, 8173, 8372, 8592, 8831, 9086, 9355, 9634, 9921, 10216, 10515, 10817, 11121,
    11425, 11729, 12033, 12334, 12633, 12930, 13223, 13513, 13799, 14082, 14361, 14636, 14907, 15174, 15437, 15696,
    15951, 16203, 16451, 16695, 16935, 17172, 17406, 17636, 17864, 18088, 18309, 18527, 18742, 18954, 19164, 19371,
    19576, 19778, 19978, 20175, 20371, 20564, 20755, 20945, 21132, 21318, 21502, 21684, 21864, 22044, 22221, 22398,
    22573, 22746, 22919, 23090, 23260, 23430, 23598, 23765, 23932, 24098, 24263, 24427, 24591, 24754, 24917, 25080,
    25242, 25404, 25566, 25727, 25889, 26051, 26212, 26374, 26536, 26699, 26862, 27026, 27190, 27355, 27521, 27688,
    27856, 28026, 28197, 28369, 28544, 28721, 28900, 29081, 29265, 29453, 29644, 29839, 30039, 30244, 30455, 30672,
    30897, 31131, 31375, 31631, 31902, 32191, 32502, 32512, 32512, 32512, 32512, 32512, 32512, 32512,
};

/* main loop (ui_input.c, with mini_poll): a sound from before CUTOFF was a parameter of its own (P_IN_CUT -1): its E7
 * (TONE then) is the CUTOFF, its RES 0: the same sound */
static void indus_poll(void)
{
    uint32_t tr;
    for (tr = 0; tr < NTRK; tr++)
        if (trk[tr].eng_req == ENGI_INDUS && trk[tr].p[P_IN_CUT] < 0) {
            trk[tr].p[P_IN_CUT] = (int16_t)clamp(trk[tr].p[P_E7], 0, 127);
            trk[tr].p[P_E7] = 0;
        }
}

static void indus_note_on(track_t *t, voice_t *v)
{
    if (!v->env_out) {
        v->ph[0] = 0;                                 /* from silence: A from its zero crossing (no click), B too */
        v->ph[1] = 0;
        v->s[1] = v->s[3] = v->s[4] = 0;
        if (!v->s[2])
            v->s[2] = 0x5F3759DF + (int32_t)v->age;
    }
    v->s[0] = (int32_t)t->p[P_E5] * 48 * 16 / 127 * 256;   /* a struck note: from P.ENV above */
}

static void indus_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    uint32_t i, pa = v->ph[0], pb = v->ph[1], ia, ib;
    int32_t pe = v->s[0], lp = v->s[1], nst = v->s[2], p16;
    int32_t mod = clamp(p[P_E1] + ((m->shape - (64 << 8)) >> 8), 0, 127), ring = p[P_E2] * 258, dry = 32767 - ring;
    int32_t nz = p[P_E3] * 200, fold = p[P_E4], g8 = 256 + fold * 6;
    int32_t own = p[P_IN_CUT] >= 0, cut = own ? p[P_IN_CUT] : p[P_E7], res = own ? clamp(p[P_E7], 0, 127) : 0;
    int32_t tone = clamp(cut + (m->cutoff >> 8), 0, 127), k = 600 + tone * tone * 2;   /* Q15: ~0.3 .. 1 */
    int32_t r1 = v->s[3], r2 = v->s[4];
    tsvf_t sv;
    if (res)                                          /* the resonance: ANALOG's SVF at the cutoff, damping 2 .. 0.05 */
        tsvf_coef_k(&sv, INDUS_SVF_CUT[tone], 8192 - res * 8000 / 127);
    {   /* the pitch envelope: exponential towards the note in P.DECAY (TIME_MS_X10), a block's step */
        int32_t kd = (int32_t)(475791u / (TIME_MS_X10[p[P_E6] & 127] + 7u));
        pe -= (int32_t)(((int64_t)pe * (kd > 65536 ? 65536 : kd)) >> 16);
    }
    p16 = m->pitch16 + (pe >> 8);
    ia = cents_inc(clamp(p16, 0, 2047), 0, m->fine);
    ib = cents_inc(clamp(p16 - 384 + p[P_E0] * 960 / 127, 0, 2047), 0, m->fine);   /* -24 .. +36 semitones */
    if (k > 32767)
        k = 32767;
    for (i = 0; i < n; i++) {
        int32_t b = sine_i(pb), a = sine_i(pa + (uint32_t)((b * mod) << 9)), s;
        s = mulq15(a, dry) + mulq15(mulq15(a, b), ring);
        if (nz)
            s += mulq15((int32_t)(noise32(&nst) >> 17) - 16384, nz);
        if (fold)
            s = sine_i((uint32_t)(s * g8) << 7);                              /* a sine folder: s at 1x a quarter turn */
        lp += ((s - lp) * k) >> 15;
        pa += ia;
        pb += ib;
        if (res) {                                    /* the peak: the band-pass at the cutoff, RES deep */
            int32_t v3 = s - r2, b1 = (sv.a1 * r1 + sv.a2 * v3) >> 13, b2 = r2 + ((sv.a2 * r1 + sv.a3 * v3) >> 13);
            r1 = clamp(2 * b1 - r1, -150000, 150000);
            r2 = clamp(2 * b2 - r2, -150000, 150000);
            out[i] += voice_amp2(soft_knee((lp + ((b1 * res) >> 6)) >> 1, 24000), m, i);
        } else
            out[i] += voice_amp2(lp >> 1, m, i);
    }
    v->s[3] = r1;
    v->s[4] = r2;
    v->ph[0] = pa;
    v->ph[1] = pb;
    v->s[0] = pe;
    v->s[1] = lp;
    v->s[2] = nst;
}

static const preset_t INDUS_PRESETS[] = {
    /* name, {RATIO, MOD, RING, NOISE, FOLD, P.ENV, P.DECAY, RES}, {A D S R}, fenv, mono, sends, more (CUTOFF: P_IN_CUT),
     * type. (Up to RES: the same values as before, CUTOFF what TONE was, RES 0: the same sounds) */
    {"FX METAL HIT", {83, 90, 30, 20, 40, 30, 50, 0}, {0, 80, 0, 70}, 0, 0, FX(20, 0, 25, 70), MORE({P_IN_CUT, 120}), TAG(PT_DRUM)},
    {"FX SHEET", {101, 110, 60, 40, 60, 10, 40, 0}, {0, 92, 0, 85}, 0, 0, FX(30, 0, 20, 90), MORE({P_IN_CUT, 127}), TAG(PT_DRUM)},
    {"FX SLAM", {60, 80, 50, 60, 90, 100, 76, 0}, {0, 96, 0, 96}, 0, 0, FX(60, 0, 30, 115), MORE({P_IN_CUT, 110}), TAG(PT_FX)},
    {"FX SIREN", {64, 30, 80, 0, 30, 0, 60, 0}, {20, 80, 127, 60}, 0, 0, FX(20, 30, 40, 60), MORE({P_LRATE, 40}, {P_LD_PIT, 40}, {P_IN_CUT, 110}), TAG(PT_FX)},
    {"FX LARSEN", {90, 40, 0, 10, 80, 0, 60, 0}, {60, 90, 127, 80}, 0, 0, FX(40, 0, 50, 80), MORE({P_LRATE, 20}, {P_LD_PIT, 3}, {P_IN_CUT, 127}), TAG(PT_FX)},
    {"FX RISER IND", {70, 30, 0, 110, 60, 0, 60, 0}, {118, 90, 127, 80}, 63, 0, FX(20, 40, 40, 100), MORE({P_IN_CUT, 56}), TAG(PT_FX)},
    {"ST INDUS", {77, 70, 40, 10, 70, 20, 40, 0}, {0, 70, 0, 55}, 0, 0, FX(40, 20, 45, 50), MORE({P_IN_CUT, 100}), TAG(PT_KEYS)},
    {"DR MACHINE", {45, 60, 70, 50, 50, 0, 60, 0}, {80, 90, 127, 100}, 0, 0, FX(20, 40, 30, 110), MORE({P_LRATE, 60}, {P_LD_SHP, 30}, {P_IN_CUT, 70}), TAG(PT_PAD)},
    /* (the resonance: a tuned feedback that the LFO sweeps; a stab that whistles) */
    {"FX LARSEN RES", {90, 30, 0, 30, 60, 0, 60, 118}, {60, 90, 127, 80}, 0, 0, FX(40, 0, 50, 80), MORE({P_LRATE, 15}, {P_LD_FLT, 25}, {P_IN_CUT, 70}), TAG(PT_FX)},
    {"ST INDUS RES", {77, 70, 40, 10, 70, 20, 40, 105}, {0, 70, 0, 55}, 0, 0, FX(40, 20, 45, 50), MORE({P_IN_CUT, 60}), TAG(PT_KEYS)},
};

static const engine_t ENG_INDUS = {
    .name = "INDUS",
    .page_title = {"METAL", "HIT"},
    .edit = {
        {"RATIO", 0, 0, 127, 77, F_INT, 0},
        {"MOD", 0, 0, 127, 60, F_PCT, 0},
        {"RING", 0, 0, 127, 0, F_PCT, 0},
        {"NOISE", 0, 0, 127, 0, F_PCT, 0},
        {"FOLD", 0, 0, 127, 20, F_PCT, 0},
        {"P.ENV", 0, 0, 127, 0, F_PCT, 0},
        {"P.DECAY", 0, 0, 127, 60, F_TIME, 0},
        {"RES", 0, 0, 127, 0, F_PCT, 0},
    },
    .presets = INDUS_PRESETS,
    .npresets = NELEM(INDUS_PRESETS),
    .note_on = indus_note_on,
    .render = indus_render,
    .knob = {P_E0, P_E1, P_E4, P_IN_CUT},
    .poly = INDUS_POLY,
    .keep = 0x00,
};
