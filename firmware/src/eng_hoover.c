/* SPDX-License-Identifier: GPL-3.0-only */
/* (techno-engines) HOOVER: the early-90s rave lead (Joey Beltram's "Mentasm", the Roland Alpha Juno's "What The"
 * patch), in hardtech, gabber and hard trance. Written for Skiff.
 *   oscillators  two PWM saws (the Alpha Juno's: a saw and a pulse of the same pitch, the pulse's width moving) DETUNE
 *                cents above and below the note, and a pulse in the middle; SUB a square an octave below
 *   PWM          each voice's own triangle (~4.5 Hz, its phase random at the note) moves the pulses' widths, as deep as
 *                PWM (the Juno's chorus-like shimmer); the track's SHP routings move the width too
 *   DIVE         every struck note starts DIVE below the note (0 .. 24 semitones) and rises to it in DIVE TIME (an
 *                exponential, ANALOG's time law: 1 ms .. 10 s), the hoover's swoop; negative routes are the track's
 *                ENV / LFO / matrix PITCH as ever. A legato note (LEG, a SLD step) glides instead (GLIDE)
 *   tone         ANALOG's soft clip (DRIVE) and its low-pass (CUTOFF RES), the track's ENV / LFO -> FLT on it
 * Width: the track's SPREAD and CHORUS (the presets use both: the Alpha Juno's chorus is half the sound). Four voices at
 * most (engine_t.poly): see livraison/CPU.md. State per voice in the part's engine area (hoover_vc_t), voice_t.s[0..1]
 * the low-pass. */
#define ENGI_HOOVER 18u          /* engines.c ENGINES[] (append-only) */
#define HOOVER_POLY 4

typedef struct {
    uint32_t ph[4];              /* PWM saw up, PWM saw down, the middle pulse, the sub */
    uint32_t lfo;                /* the PWM triangle's phase */
    int32_t dive;                /* the pitch below the note, 1/16 semitone << 8 (0: arrived) */
} hoover_vc_t;

static hoover_vc_t *hoover_vc(const track_t *t, const voice_t *v)
{
    return &((hoover_vc_t *)eng_part(t))[(uint32_t)(v - t->v) % NVOICE];
}

static void hoover_note_on(track_t *t, voice_t *v)
{
    hoover_vc_t *H = hoover_vc(t, v);
    if (!v->env_out) {                                /* from silence: fresh phases, a random PWM start */
        int32_t seed = (int32_t)(0x6C8E9CF5u ^ (v->age * 2246822519u));
        H->ph[0] = noise32(&seed);
        H->ph[1] = noise32(&seed);
        H->ph[2] = noise32(&seed);
        H->ph[3] = H->ph[2] >> 1;
        H->lfo = noise32(&seed);
        v->s[0] = v->s[1] = 0;
    }
    H->dive = -(int32_t)t->p[P_E0] * 24 * 16 / 127 * 256;   /* a struck note: from DIVE below (a legato one: no note_on) */
}

/* the Alpha Juno's PWM saw: its saw and its pulse of width pw, half each */
static inline int32_t hoover_pwsaw(uint32_t ph, uint32_t inc, uint32_t pw)
{
    return (osc_saw(ph, inc) + osc_pulse(ph, inc, pw)) >> 1;
}

static void hoover_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    hoover_vc_t *H = hoover_vc(t, v);
    uint32_t i, ph0 = H->ph[0], ph1 = H->ph[1], ph2 = H->ph[2], ph3 = H->ph[3], inc0, inc1, inc2, inc3, pwa, pwb;
    int32_t p16, det = p[P_E3] / 2, tri, depth, sl = p[P_E6] * 200, drv = p[P_E7], drive = 32768 + drv * 512;
    int32_t cut = (p[P_E4] << 8) + m->cutoff + ((v->pitch16 - 60 * 16) * 32 >> 4);   /* (half key tracking) */
    int32_t ic1 = v->s[0], ic2 = v->s[1];
    tsvf_t flt;
    {   /* the dive: an exponential towards the note, DIVE TIME (ANALOG's time law, TIME_MS_X10) */
        int32_t k = (int32_t)(475791u / (TIME_MS_X10[p[P_E1] & 127] + 7u));   /* Q16, a block's step (0.726 ms) */
        H->dive -= (int32_t)(((int64_t)H->dive * (k > 65536 ? 65536 : k)) >> 16);
    }
    p16 = m->pitch16 + (H->dive >> 8);
    inc0 = cents_inc(clamp(p16, 0, 2047), det, m->fine);
    inc1 = cents_inc(clamp(p16, 0, 2047), -det, m->fine);
    inc2 = cents_inc(clamp(p16, 0, 2047), 0, m->fine);
    inc3 = inc2 >> 1;
    {   /* the PWM triangle (~4.5 Hz), a block's step; the track's SHP routings on the centre */
        uint32_t li = 438272u * CTL;
        H->lfo += li;
        tri = (int32_t)(H->lfo >> 15);
        tri = tri < 65536 ? tri - 32768 : 98303 - tri;                          /* -32768 .. 32767 */
        depth = p[P_E2] * 200;                                                  /* up to ~40 % of the cycle */
        pwa = 0x80000000u + (uint32_t)((mulq15(tri, depth) + (m->shape - (64 << 8))) << 15);
        pwb = 0x80000000u - (uint32_t)((mulq15(tri, depth) - (m->shape - (64 << 8))) << 15);
    }
    tsvf_coef(&flt, cut, p[P_E5]);
    for (i = 0; i < n; i++) {
        int32_t s = (hoover_pwsaw(ph0, inc0, pwa) + hoover_pwsaw(ph1, inc1, pwb) + osc_pulse(ph2, inc2, pwb)) * 11 >> 5;
        ph0 += inc0;
        ph1 += inc1;
        ph2 += inc2;
        if (sl) {
            s += mulq15(osc_pulse(ph3, inc3, 0x80000000u), sl) >> 1;
            ph3 += inc3;
        }
        if (drv)
            s = softclip(((s >> 2) * (drive >> 2)) >> 11);
        s = soft_knee(tsvf_lp(&flt, s >> 1, &ic1, &ic2), 16000) << 1;
        out[i] += voice_amp2(s, m, i);
    }
    H->ph[0] = ph0;
    H->ph[1] = ph1;
    H->ph[2] = ph2;
    H->ph[3] = ph3;
    v->s[0] = ic1;
    v->s[1] = ic2;
}

static const preset_t HOOVER_PRESETS[] = {
    /* name, {DIVE, D.TIME, PWM, DETUNE, CUTOFF, RES, SUB, DRIVE}, {A D S R}, fenv, mono, sends, more, type */
    {"LD MENTASM", {50, 74, 90, 44, 104, 15, 40, 10}, {2, 70, 115, 55}, 0, 1, FX(10, 110, 30, 45), MORE({P_SPRD, 110}, {P_GLIDE, 24}), TAG(PT_LEAD)},
    {"LD HOOV HARD", {60, 70, 100, 50, 110, 20, 60, 80}, {0, 70, 115, 45}, 0, 1, FX(70, 90, 20, 30), MORE({P_SPRD, 100}, {P_DTYPE, 1}, {P_GLIDE, 20}), TAG(PT_LEAD)},
    {"LD HOOV DARK", {44, 80, 80, 40, 70, 25, 70, 20}, {2, 72, 115, 60}, 10, 1, FX(20, 100, 25, 60), MORE({P_SPRD, 110}, {P_GLIDE, 24}), TAG(PT_LEAD)},
    {"LD GABBER HV", {70, 60, 110, 60, 112, 18, 70, 110}, {0, 70, 115, 40}, 0, 1, FX(100, 60, 15, 20), MORE({P_DTYPE, 1}, {P_SPRD, 90}), TAG(PT_LEAD)},
    {"LD RAVE HV", {36, 66, 70, 34, 100, 12, 20, 0}, {4, 72, 110, 55}, 0, 0, FX(0, 90, 50, 50), MORE({P_SPRD, 120}), TAG(PT_LEAD)},
    {"ST HOOVER", {40, 60, 90, 44, 96, 20, 30, 20}, {0, 68, 0, 50}, 20, 0, FX(15, 90, 55, 55), MORE({P_SPRD, 120}), TAG(PT_KEYS)},
    {"BS HOOVER", {30, 64, 60, 30, 70, 15, 90, 40}, {0, 64, 100, 22}, 0, 1, FX(30, 40, 10, 5), TAG(PT_BASS)},
    {"LD HOOV TRNC", {24, 60, 76, 38, 108, 14, 30, 30}, {2, 72, 112, 58}, 0, 0, FX(20, 100, 50, 60), MORE({P_SPRD, 120}), TAG(PT_LEAD)},
};

static const engine_t ENG_HOOVER = {
    .name = "HOOVER",
    .page_title = {"HOOVER", "TONE"},
    .edit = {
        {"DIVE", 0, 0, 127, 40, F_PCT, 0},
        {"D.TIME", 0, 0, 127, 70, F_TIME, 0},
        {"PWM", 0, 0, 127, 80, F_PCT, 0},
        {"DETUNE", 0, 0, 127, 40, F_INT, U_CT},
        {"CUTOFF", 0, 0, 127, 100, F_CUTOFF, 0},
        {"RES", 0, 0, 127, 15, F_PCT, 0},
        {"SUB", 0, 0, 127, 0, F_PCT, 0},
        {"DRIVE", 0, 0, 127, 0, F_PCT, 0},
    },
    .presets = HOOVER_PRESETS,
    .npresets = NELEM(HOOVER_PRESETS),
    .note_on = hoover_note_on,
    .render = hoover_render,
    .knob = {P_E0, P_E2, P_E3, P_E4},
    .poly = HOOVER_POLY,
    .keep = 0x03,                /* the low-pass */
};
