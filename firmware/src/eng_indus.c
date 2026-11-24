/* SPDX-License-Identifier: GPL-3.0-only */
/* (techno-engines) INDUS: industrial textures, metal and machines: a light engine. Written for Skiff.
 *   two sines, A at the note and B at RATIO (0.25 .. 8 times the note, on a continuous scale: inharmonic between the
 *   whole ratios, -24 .. +36 semitones, 64 ~ x 1.42); MOD: B modulates A's phase (cross / FM, up to ~4 radians: bells,
 *   sheet metal); RING: A x B against A (ring modulation: sirens, robots); NOISE: white noise under it; FOLD: a sine
 *   wavefolder (1x .. ~4x: grit, harsh harmonics); P.ENV / P.DECAY: every struck note starts P.ENV above the note (0 ..
 *   48 semitones) and falls to it in P.DECAY (an exponential, ANALOG's time law): impacts, zaps, machine hits; TONE: a
 *   one-pole low-pass (127: open).
 * The track's SHP routings move MOD (the matrix, ENV / LFO DEST SHAPE: a sweeping bell), its FLT routings TONE. Six voices
 * at most (engine_t.poly: see livraison/CPU.md); per voice: v->ph[0] A, v->ph[1] B, s[0] the pitch envelope (1/16
 * semitone << 8), s[1] the low-pass, s[2] the noise. */
/* ENGI_INDUS 19 (core.h; engines.c ENGINES[], append-only) */
#define INDUS_POLY 6

static void indus_note_on(track_t *t, voice_t *v)
{
    if (!v->env_out) {
        v->ph[0] = 0;                                 /* from silence: A from its zero crossing (no click), B too */
        v->ph[1] = 0;
        v->s[1] = 0;
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
    int32_t tone = clamp(p[P_E7] + (m->cutoff >> 8), 0, 127), k = 600 + tone * tone * 2;   /* Q15: ~0.3 .. 1 */
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
        out[i] += voice_amp2(lp >> 1, m, i);
    }
    v->ph[0] = pa;
    v->ph[1] = pb;
    v->s[0] = pe;
    v->s[1] = lp;
    v->s[2] = nst;
}

static const preset_t INDUS_PRESETS[] = {
    /* name, {RATIO, MOD, RING, NOISE, FOLD, P.ENV, P.DECAY, TONE}, {A D S R}, fenv, mono, sends, more, type */
    {"FX METAL HIT", {83, 90, 30, 20, 40, 30, 50, 120}, {0, 80, 0, 70}, 0, 0, FX(20, 0, 25, 70), TAG(PT_DRUM)},
    {"FX SHEET", {101, 110, 60, 40, 60, 10, 40, 127}, {0, 92, 0, 85}, 0, 0, FX(30, 0, 20, 90), TAG(PT_DRUM)},
    {"FX SLAM", {60, 80, 50, 60, 90, 100, 76, 110}, {0, 96, 0, 96}, 0, 0, FX(60, 0, 30, 115), TAG(PT_FX)},
    {"FX SIREN", {64, 30, 80, 0, 30, 0, 60, 110}, {20, 80, 127, 60}, 0, 0, FX(20, 30, 40, 60), MORE({P_LRATE, 40}, {P_LD_PIT, 40}), TAG(PT_FX)},
    {"FX LARSEN", {90, 40, 0, 10, 80, 0, 60, 127}, {60, 90, 127, 80}, 0, 0, FX(40, 0, 50, 80), MORE({P_LRATE, 20}, {P_LD_PIT, 3}), TAG(PT_FX)},
    {"FX RISER IND", {70, 30, 0, 110, 60, 0, 60, 56}, {118, 90, 127, 80}, 63, 0, FX(20, 40, 40, 100), TAG(PT_FX)},
    {"ST INDUS", {77, 70, 40, 10, 70, 20, 40, 100}, {0, 70, 0, 55}, 0, 0, FX(40, 20, 45, 50), TAG(PT_KEYS)},
    {"DR MACHINE", {45, 60, 70, 50, 50, 0, 60, 70}, {80, 90, 127, 100}, 0, 0, FX(20, 40, 30, 110), MORE({P_LRATE, 60}, {P_LD_SHP, 30}), TAG(PT_PAD)},
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
        {"TONE", 0, 0, 127, 110, F_PCT, 0},
    },
    .presets = INDUS_PRESETS,
    .npresets = NELEM(INDUS_PRESETS),
    .note_on = indus_note_on,
    .render = indus_render,
    .knob = {P_E0, P_E1, P_E4, P_E3},
    .poly = INDUS_POLY,
    .keep = 0x00,
};
