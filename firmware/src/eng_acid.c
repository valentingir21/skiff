/* SPDX-License-Identifier: GPL-3.0-only */
/* (techno-engines) ACID: a TB-303 voice, the ATEK303 voice in integers.
 * The voice is ATEK303's (animatek/FM1-Animatek, atek303/app/dsp/atek_voice.c, GPL-3.0-or-later, Copyright (C) 2026
 * Javier Melgar (Animatek); "the TB-303 voice of the ATEK303 VCV Rack module"), its Open303 parts by Robin Schmidt
 * (MIT, LICENSES/MIT-Open303.txt). ATEK303 runs it in single-precision float, which Skiff has not on the chip: here the
 * same equations in fixed point, their laws worked out once by tools/gen_acid_tables.py (acid/eng_acid_tables.h).
 * Piece by piece, as atek_voice.c has them:
 *   the oscillator  ATEK's: a polyBLEP ramp and a comparator square (pulse width 44 %), the ramp's reset rounded (20 us),
 *                   the square's edges slewed (5 us), both AC-coupled (the droop: 15 Hz, 60 Hz), run at 4x. WAVE SAW or
 *                   SQR, and two of ATEK's layer recipes, SAW+SUB and SQR+SUB (a sine an octave down at 50 %)
 *   the filter      ATEK's diode ladder: four cells, a soft diode (1.2 tanh(x / 1.2)) in every one, the first cell's
 *                   capacitor 33/18 of the others', the resonance fed back through a 150 Hz high-pass, +6 dB of drive,
 *                   its b0 and feedback law Open303's (the measured TB-303 mapping); at 4x, a 44 Hz high-pass in front
 *   Open303         the filter envelope (a pure decay: DECAY 200 ms .. 2.5 s as the schematic's, an accented note 200 ms),
 *                   ENV MOD's log pot and Open303's measured cutoff mapping (CUTOFF 314 .. 2394 Hz), the accent's 15 ms RC
 *                   into the cutoff, the amp envelope (1.23 s decay while held, released in 1 ms, an accented note in
 *                   50 ms) with the filter envelope added to it (the 303's accented VCA), its 200 Hz de-click, then the
 *                   fixed allpass (14 Hz), high-pass (24 Hz) and notch (7.5 Hz); the OTA's soft headroom; the output's
 *                   soft saturation
 *   ATEK's module   accent accumulation (two charges, 100 ms and 450 ms, each accent adds 0.55: close accents open more:
 *                   the 303's "wow"), the slide in the pitch domain (an exponential glide, SLIDE its time constant,
 *                   ATEK's default 120 ms)
 * Departures, on purpose: integers; the 12th-order elliptic anti-aliasing filter of Open303 is two half-band decimators
 * here (4x -> 2x -> 1x, 19 and 51 taps, flat to 18 kHz, what folds back below 18 kHz under -70 dB: the same job for a
 * tenth of the multiplies); the coefficients follow the cutoff from tables (1/32 octave, linear between) instead of the
 * formulas a sample; ATEK's analog drift and the rest of its layer list are left out; the parameters are read once a
 * block (32 samples; ATEK: 16).
 * Skiff's side: the accent is the sequencer step's ACC (seq_accent[], set by seq.c as the step plays), never the
 * velocity: a key played hard does not accent. A SLD step (or overlapping keys: the presets are LEGATO) slides there
 * without restarting the envelopes. The voice's envelopes are its own (engine_t.ownenv: the track's ADSR does not
 * shape it; the 303 has none). One voice (a 303 is monophonic). State in the part's engine area (acid_vc_t). */
#define ENGI_ACID 16u            /* engines.c ENGINES[] (append-only) */
static const char *const N_ACID_WAVE[] = {"SAW", "SQR", "SAW+SUB", "SQR+SUB"};
#include "acid/eng_acid_tables.h"

#define ACID_SUB 16              /* the sub-block the decimators work on (output samples) */
typedef struct {
    uint32_t ph, phs;                       /* the oscillator's phase (at ACID_OS x), the sub's */
    int32_t lp, hy, hx;                     /* the wave's edge filter and AC coupling (Q16) */
    int32_t h1x, h1y;                       /* the 44 Hz high-pass in front of the ladder */
    int32_t y1, y2, y3, y4, fb, fbp;        /* the ladder's cells, the feedback's high-pass */
    int32_t d2[ACID_HB2_N - 1];             /* the decimator's history */
    int32_t apx, apy, hpx, hpy, nx1, nx2, ny1, ny2, cx1, cx2, cy1, cy2;   /* allpass, high-pass, notch, de-click */
    int32_t ne, ce;                         /* the notch's and the de-click's rounding error, fed back (their poles
                                             * sit by 1: a dropped residue would build up as DC) */
    int32_t env, envc;                      /* the filter envelope (Q31) and its factor a sample */
    int32_t rc2;                            /* the accent's RC (Q16) */
    int32_t amp, relc;                      /* the amp envelope (Q31), its release factor */
    int32_t accg;                           /* this note's accent (Q16, 0: none) */
    int32_t accf, accs;                     /* the accent's two charges (Q16) */
    int32_t slew;                           /* the sliding pitch, 1/16 semitone Q8 */
    int32_t lcs;                            /* CUTOFF, smoothed (log2 Hz, Q16) */
    uint8_t trig, gate, note, ampon, wave;
} acid_vc_t;

#define ACID_NVC 2                /* one voice plays (engine_t.poly 1); a second state for the one a steal fades out */
static acid_vc_t *acid_vc(const track_t *t, const voice_t *v)
{
    return &((acid_vc_t *)eng_part(t))[(uint32_t)(v - t->v) % ACID_NVC];
}

static inline int32_t mulq31(int32_t a, int32_t b) { return (int32_t)(((int64_t)a * b + (1 << 30)) >> 31); }   /* (rounded:
                                                 * the one-poles near 1 would turn a floor's bias into DC) */
static inline int32_t acid_diode(int32_t x)             /* 1.2 tanh(x / 1.2), Q16 */
{
    int32_t a = x < 0 ? -x : x, i = a >> ACID_DIODE_SHIFT, y;
    y = i >= ACID_DIODE_N ? ACID_DIODE[ACID_DIODE_N]
                          : ACID_DIODE[i] + (((ACID_DIODE[i + 1] - ACID_DIODE[i]) * (a & 1023)) >> 10);
    return x < 0 ? -y : y;
}
static inline int32_t acid_tanh(int32_t x)              /* tanh(x), Q16 */
{
    int32_t a = x < 0 ? -x : x, i = a >> 11, y;
    y = i >= 256 ? ACID_TANH[256] : ACID_TANH[i] + (((ACID_TANH[i + 1] - ACID_TANH[i]) * (a & 2047)) >> 11);
    return x < 0 ? -y : y;
}

/* an accent from the sequencer (ACC), taken by the note it belongs to: set_accent_and_decay; a struck note (strike)
 * also adds to the charges (at_gate: a gate going up), a slide into an accented note does not (its auto legato) */
static void acid_accent(track_t *t, acid_vc_t *A, int strike)
{
    uint32_t k = (uint32_t)(t - trk) % NTRK;
    int32_t acc = seq_accent[k];
    seq_accent[k] = 0;
    if (acc) {
        int32_t base = t->p[P_E5] * 516, amt = (A->accf + A->accs) >> 1, a;   /* (516: 127 -> 65532) */
        a = base + (int32_t)(((int64_t)base * ((amt * 52429) >> 16)) >> 16);   /* x (1 + 0.8 x the charges) */
        A->accg = a > 65536 ? 65536 : a;
        if (strike) {
            A->accf = A->accf + 36045 > 65536 ? 65536 : A->accf + 36045;      /* + 0.55 each */
            A->accs = A->accs + 36045 > 65536 ? 65536 : A->accs + 36045;
        }
        A->envc = (int32_t)ACID_MAINC[0];                                       /* the 303's shortest decay */
        A->relc = 973695;                                                       /* 50 ms */
    } else {
        A->accg = 0;
        A->envc = (int32_t)ACID_MAINC[clamp(t->p[P_E4], 0, 127)];
        A->relc = 48147818;                                                     /* 1 ms */
    }
}

static void acid_note_on(track_t *t, voice_t *v)
{
    acid_vc_t *A = acid_vc(t, v);
    if (!v->env_out) {                                /* from silence: clean states (the accent's charges live on) */
        int32_t f = A->accf, s = A->accs;
        memset(A, 0, sizeof *A);
        A->accf = f;
        A->accs = s;
    }
    acid_accent(t, A, 1);
    A->env = 0x7FFFFFFF;                              /* the filter envelope from the top */
    A->amp = 0x7FFFFFFF;                              /* the amp envelope: attack 0 */
    A->ampon = 1;
    A->trig = 1;
    A->gate = 1;
    A->note = v->note;
}

static int acid_done(track_t *t, voice_t *v)
{
    const acid_vc_t *A = acid_vc(t, v);
    return !v->gate && !A->ampon && A->amp < (1 << 17);
}

static void acid_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    acid_vc_t *A = acid_vc(t, v);
    int32_t b2[ACID_HB2_N - 1 + 2 * ACID_SUB], ampb[ACID_SUB];
    int32_t r16, es16, off16, base, c16, lcx, slo, shi, e16, x16, wave, sub, sq, pw, inco;
    int32_t b0 = 0, b0c = 0, k16 = 0, og = 0;
    uint32_t i, j, done = 0;
    if (n > CTL)
        n = CTL;
    /* the gate's events: a slide (a new note, the gate held, no note-on), a release */
    if (!A->trig && v->gate && v->note != A->note) {
        acid_accent(t, A, 0);
        A->note = v->note;
    }
    if (A->gate && !v->gate)
        A->ampon = 0;
    A->gate = v->gate;
    A->accf = mulq31(A->accf, 2131955645);            /* the charges drain: 100 ms, 450 ms (a block) */
    A->accs = mulq31(A->accs, 2144023541);
    {   /* the pitch: the note slides (ATEK's slew on the pitch, SLIDE its time constant), the host's bend / LFO on top */
        int32_t tg = v->pitch16 << 8, p16, fine, tau = 30 + p[P_E7] * 2;   /* (the note itself: the track's GLIDE
                                                         * does not add to SLIDE) */
        if (A->trig)
            A->slew = tg;
        else
            A->slew += (int32_t)(((int64_t)(tg - A->slew) * (47554 / tau)) >> 16);
        p16 = (A->slew >> 8) + (m->pitch16 - v->pitch_cur);
        fine = m->fine + p[P_E6] * 2367 / 1000 + (((A->slew & 255) * 59) >> 10);
        inco = (int32_t)(cents_inc(clamp(p16, 0, 2047), 0, fine) / ACID_OS);
    }
    /* CUTOFF (log2 Hz, 314 .. 2394), smoothed (~20 ms); ENV MOD's log pot and Open303's scaler and offset; RES */
    base = 543596 + p[P_E1] * 192059 / 127;
    if (A->trig && !A->lcs)
        A->lcs = base;
    A->lcs += (int32_t)(((int64_t)(base - A->lcs) * 2365) >> 16);
    c16 = clamp((int32_t)(((int64_t)(A->lcs - 543541) * 22355) >> 16), 0, 65536);   /* (/ log2(2394 / 314)) */
    x16 = p[P_E3] * 516;
    e16 = (int32_t)(((int64_t)((x16 * (int64_t)x16) >> 16) * x16) >> 16);
    slo = (int32_t)((247336LL * e16) >> 16) + 48297;
    shi = (int32_t)((274897LL * e16) >> 16) + 56646;
    es16 = (int32_t)((((int64_t)(65536 - c16) * slo) + (int64_t)c16 * shi) >> 16);
    off16 = ((3165 * c16) >> 16) + 19293;
    r16 = p[P_E2] * 516;
    lcx = (m->cutoff * 4681) >> 8;                    /* the host's (LFO, matrix, ENV routings): 1/14 octave a step */
    wave = clamp(p[P_E0], 0, 3);
    sq = wave & 1;
    sub = wave >> 1;
    if (wave != A->wave)
        A->wave = (uint8_t)wave, A->lp = A->hy = A->hx = 0;
    pw = 1889785610;                                  /* 44 % of the turn: the square's falling edge */
    A->trig = 0;

    while (done < n) {
        uint32_t ns = n - done < ACID_SUB ? n - done : ACID_SUB;
        memcpy(b2, A->d2, sizeof A->d2);
        for (i = 0; i < ns; i++) {
            int32_t m16, tg, ampo;
            A->env = mulq31(A->env, A->envc);
            m16 = A->env >> 15;
            tg = A->accg ? m16 : 0;
            A->rc2 = tg + mulq31(A->rc2 - tg, 2144239716);                     /* 15 ms */
            if (!(i & 3u)) {    /* the cutoff, from CUTOFF, the filter envelope and the accent: the ladder's
                                 * coefficients every 4th sample (ATEK: every sample; the envelope moves slowly) */
                int32_t lc, ix, fr, ks16, g16;
                lc = A->lcs + lcx + (int32_t)(((int64_t)es16 * (m16 - off16)) >> 16) + (int32_t)(((int64_t)A->accg * A->rc2) >> 16);
                ix = clamp(lc - ACID_LC0, 0, (ACID_NC - 1) * 2048 - 1);
                fr = ix & 2047;
                ix >>= 11;
                b0 = (int32_t)ACID_B0[ix] + ((((int32_t)ACID_B0[ix + 1] - (int32_t)ACID_B0[ix]) * fr) >> 11);
                ks16 = ((int32_t)ACID_KS[ix] + ((((int32_t)ACID_KS[ix + 1] - (int32_t)ACID_KS[ix]) * fr) >> 11)) << 6;
                b0c = b0 + (int32_t)(((int64_t)b0 * 54613) >> 16);             /* x 33/18: the first cell's capacitor */
                k16 = (int32_t)(((int64_t)ks16 * r16) >> 16);
                if (k16 > 40 << 16)
                    k16 = 40 << 16;
                g16 = (int32_t)((((int64_t)(((int64_t)ks16 * 3855) >> 16) - 65536) * r16) >> 16) + 65536;   /* (ks / 17 - 1) r + 1 */
                og = (int32_t)(((int64_t)g16 * (65536 + r16)) >> 15);             /* 2 x the gain compensation */
            }
            /* the amp envelope, the accented VCA (the filter envelope added), the de-click */
            A->amp -= mulq31(A->amp, A->ampon ? 39590 : A->relc);
            ampo = (A->amp >> 15) + (A->ampon ? (int32_t)(((int64_t)(29491 + 4 * A->accg) * m16) >> 16) : 0);
            {
                int64_t y = 213644LL * ampo + 427288LL * A->cx1 + 213644LL * A->cx2 + 2104219405LL * A->cy1 - 1031332157LL * A->cy2 + A->ce;
                A->cx2 = A->cx1;
                A->cx1 = ampo;
                A->cy2 = A->cy1;
                A->cy1 = (int32_t)(y >> 30);
                A->ce = (int32_t)(y - ((int64_t)A->cy1 << 30));
                ampb[i] = A->cy1;
            }
            /* oversampled: the oscillator, the 44 Hz high-pass, the ladder */
            for (j = 0; j < ACID_OS; j++) {
                uint32_t ph = A->ph += (uint32_t)inco, d = (uint32_t)inco >> 16;
                int32_t s, x, y;
                if (!sq) {                                                       /* the ramp */
                    s = (int32_t)(ph >> 15) - 65536;
                    if (d) {
                        if (ph < (uint32_t)inco) {
                            int32_t u = (int32_t)(ph / d);
                            s -= u + u - (int32_t)(((int64_t)u * u) >> 16) - 65536;
                        } else if (ph > ~(uint32_t)inco) {
                            int32_t u = -(int32_t)(~ph / d);
                            s -= (int32_t)(((int64_t)u * u) >> 16) + u + u + 65536;
                        }
                    }
                    A->lp += mulq31(s - A->lp, ACID_LP_SAW);                     /* the reset's 20 us */
                    A->hy = mulq31(A->hy + A->lp - A->hx, ACID_HP_SAW);          /* 15 Hz droop */
                } else {                                                         /* the comparator's square */
                    uint32_t tf = ph - (uint32_t)pw;
                    s = ph < (uint32_t)pw ? 65536 : -65536;
                    if (d) {
                        if (ph < (uint32_t)inco) {
                            int32_t u = (int32_t)(ph / d);
                            s += u + u - (int32_t)(((int64_t)u * u) >> 16) - 65536;
                        } else if (ph > ~(uint32_t)inco) {
                            int32_t u = -(int32_t)(~ph / d);
                            s += (int32_t)(((int64_t)u * u) >> 16) + u + u + 65536;
                        }
                        if (tf < (uint32_t)inco) {
                            int32_t u = (int32_t)(tf / d);
                            s -= u + u - (int32_t)(((int64_t)u * u) >> 16) - 65536;
                        } else if (tf > ~(uint32_t)inco) {
                            int32_t u = -(int32_t)(~tf / d);
                            s -= (int32_t)(((int64_t)u * u) >> 16) + u + u + 65536;
                        }
                    }
                    A->lp += mulq31(s - A->lp, ACID_LP_SQ);                      /* the edges' 5 us */
                    A->hy = mulq31(A->hy + A->lp - A->hx, ACID_HP_SQ);           /* 60 Hz droop */
                }
                A->hx = A->lp;
                s = A->hy;
                if (sub) {                                                       /* the layer's sine, an octave down, 50 % */
                    A->phs += (uint32_t)inco >> 1;
                    s += osc_sine(A->phs);
                }
                s = -((s * 31) >> 6);                                            /* OSC_GAIN 0.484, inverted */
                y = mulq31(s - A->h1x, ACID_HP1_B0) + mulq31(A->h1y, ACID_HP1_A);   /* 44.5 Hz */
                A->h1x = s;
                A->h1y = y;
                {   /* the diode ladder */
                    int32_t f = (int32_t)(((int64_t)k16 * acid_diode(A->y4)) >> 16);
                    A->fb = mulq31(A->fb + f - A->fbp, ACID_FBHP);               /* 150 Hz */
                    A->fbp = f;
                    x = 2 * y - A->fb;                                           /* +6 dB of drive */
                    A->y1 += (int32_t)(((int64_t)b0c * acid_diode(x - A->y1 + A->y2)) >> 24);
                    A->y2 += (int32_t)(((int64_t)b0 * acid_diode(A->y1 - 2 * A->y2 + A->y3)) >> 24);
                    A->y3 += (int32_t)(((int64_t)b0 * acid_diode(A->y2 - 2 * A->y3 + A->y4)) >> 24);
                    A->y4 += (int32_t)(((int64_t)b0 * acid_diode(A->y3 - 2 * A->y4)) >> 24);
                    if ((uint32_t)(A->y1 + (1 << 26)) > (2u << 26) || (uint32_t)(A->y4 + (1 << 26)) > (2u << 26))
                        A->y1 = A->y2 = A->y3 = A->y4 = A->fb = A->fbp = 0;     /* (runaway) */
                    b2[ACID_HB2_N - 1 + ACID_OS * i + j] = (int32_t)(((int64_t)og * A->y4) >> 16);
                }
            }
        }
        /* the decimator, 2x -> 1x; then at 1x the allpass (14 Hz), the high-pass (24 Hz), the notch (7.5 Hz), the OTA,
         * the amp, the output's saturation */
        for (i = 0; i < ns; i++) {
            const int32_t *x = &b2[2 * i + 1 + (ACID_HB2_N - 1) / 2];
            int64_t a = (int64_t)x[0] << 14, y;
            int32_t s, o;
            for (j = 0; j < NELEM(ACID_HB2); j++)
                a += (int64_t)ACID_HB2[j] * (x[-(int32_t)(2 * j + 1)] + x[2 * j + 1]);
            s = (int32_t)(a >> 15);
            o = mulq31(s, -2143201968) + A->apx + mulq31(A->apy, 2143201968);
            A->apx = s;
            A->apy = o;
            s = mulq31(o - A->hpx, 2143792883) + mulq31(A->hpy, 2140102118);
            A->hpx = o;
            A->hpy = s;
            y = 1070930800LL * s - 2141860373LL * A->nx1 + 1070930800LL * A->nx2 + 2141860373LL * A->ny1 - 1068119777LL * A->ny2 + A->ne;
            A->nx2 = A->nx1;
            A->nx1 = s;
            A->ny2 = A->ny1;
            A->ny1 = s = (int32_t)(y >> 30);
            A->ne = (int32_t)(y - ((int64_t)s << 30));
            s = (acid_tanh((s * 5) / 3) * 39322) >> 16;                         /* the OTA: 0.6 tanh(x / 0.6) */
            s = (int32_t)(((int64_t)s * ampb[i]) >> 16);
            s = (int32_t)(((int64_t)s * 164233) >> 16);                         /* x -6 dB x 5 (the module's volts) */
            s = 7 * acid_tanh((int32_t)(((int64_t)s * 9362) >> 16));            /* 7 tanh(x / 7) (always under 7: */
            s = (s * 13107) >> 17;                          /* no clamp needed); / 5 (back from the volts), Q16 -> Q15 */
            out[done + i] += voice_amp2(s, m, done + i);    /* (x 2: level with ANALOG's basses and the ACID before) */
        }
        memcpy(A->d2, &b2[2 * ns], sizeof A->d2);
        done += ns;
    }
}

static const preset_t ACID_PRESETS[] = {
    /* name, {WAVE, CUTOFF, RES, ENV MOD, DECAY, ACCENT, TUNE, SLIDE}, {A D S R}, fenv, mono, sends, more, type.
     * (the ADSR: unused, the 303's envelopes are its own) */
    {"BS 303", {0, 30, 95, 80, 60, 80, 0, 45}, {0, 100, 127, 12}, 0, 1, FX(0, 0, 30, 10), TAG(PT_BASS)},
    {"BS 303 RESO", {0, 24, 118, 70, 66, 90, 0, 45}, {0, 100, 127, 12}, 0, 1, FX(0, 0, 40, 15), TAG(PT_BASS)},
    {"BS 303 TEAR", {0, 100, 100, 100, 20, 120, 0, 30}, {0, 100, 127, 10}, 0, 1, FX(127, 0, 20, 8), MORE({P_DTYPE, 4}, {P_DTONE, 16}), TAG(PT_BASS)},
    {"BS 303 DARK", {1, 14, 90, 55, 70, 60, 0, 45}, {0, 100, 127, 14}, 0, 1, FX(0, 0, 20, 20), TAG(PT_BASS)},
    {"BS 303 SQLCH", {1, 30, 127, 127, 20, 127, 0, 45}, {0, 100, 127, 10}, 0, 1, FX(10, 0, 30, 10), TAG(PT_BASS)},
    {"BS PHUTURE", {0, 10, 112, 127, 127, 95, 0, 60}, {0, 100, 127, 14}, 0, 1, FX(10, 0, 90, 40), TAG(PT_BASS)},
    {"BS 303 DIST", {1, 34, 110, 90, 56, 110, 0, 45}, {0, 100, 127, 10}, 0, 1, FX(70, 0, 15, 8), MORE({P_DTYPE, 4}), TAG(PT_BASS)},
    {"LD 303 TRNC", {0, 40, 100, 75, 80, 70, 0, 45}, {0, 100, 127, 20}, 0, 1, FX(0, 30, 60, 40), TAG(PT_LEAD)},
    {"BS 303 SUB", {2, 22, 100, 70, 50, 85, 0, 45}, {0, 100, 127, 12}, 0, 1, FX(0, 0, 20, 10), TAG(PT_BASS)},
    {"BS 303 DUB", {3, 4, 30, 127, 10, 76, 0, 70}, {0, 100, 127, 12}, 0, 1, FX(0, 0, 55, 30), TAG(PT_BASS)},
    /* (techno-presets-v2) the second techno bank (tools/bank_v2.py, livraison/PRESETS.md) */
    {"PL 303", {0, 80, 120, 30, 20, 90, 0, 20}, {0, 100, 127, 8}, 0, 1, FX(0, 0, 90, 40), TAG(PT_PLUCK)},
    {"LD 303 SCRM", {1, 90, 127, 90, 60, 110, 0, 50}, {0, 100, 127, 20}, 0, 1, FX(60, 0, 90, 30), MORE({P_DTYPE, 1}), TAG(PT_LEAD)},
    {"BS 303 FUNK", {1, 60, 40, 40, 100, 60, 0, 127}, {0, 100, 127, 14}, 0, 1, FX(0, 50, 30, 10), TAG(PT_BASS)},
    {"BS 303 MUTED", {2, 10, 40, 40, 40, 40, 0, 45}, {0, 100, 127, 12}, 0, 1, FX(0, 0, 10, 10), TAG(PT_BASS)},
    {"BS 303 CRUSH", {2, 40, 100, 80, 50, 90, 0, 45}, {0, 100, 127, 10}, 0, 1, FX(10, 0, 30, 10), MORE({P_ITYPE, 5}, {P_IA, 24}, {P_IB, 30}, {P_IC, 120}), TAG(PT_BASS)},
    {"BS 303 PHASR", {0, 30, 100, 80, 60, 80, 0, 45}, {0, 100, 127, 12}, 0, 1, FX(0, 0, 30, 20), MORE({P_ITYPE, 6}, {P_IA, 60}, {P_IB, 120}, {P_IC, 110}), TAG(PT_BASS)},
    {"LD 303 FOLD", {0, 50, 110, 80, 60, 100, 0, 45}, {0, 100, 127, 16}, 0, 1, FX(20, 0, 60, 30), MORE({P_ITYPE, 3}, {P_IA, 110}, {P_IB, 90}), TAG(PT_LEAD)},
    {"BS 303 GATE", {1, 30, 100, 80, 80, 80, 0, 45}, {0, 100, 127, 12}, 0, 1, FX(10, 0, 30, 10), MORE({P_LRATE, 80}, {P_LSYNC, 10}, {P_LD_AMP, 100}), TAG(PT_BASS)},
    {"PL 303 FLNG", {0, 60, 110, 60, 20, 90, 0, 20}, {0, 100, 127, 8}, 0, 1, FX(0, 0, 70, 50), MORE({P_ITYPE, 7}, {P_IA, 40}, {P_IB, 120}, {P_IC, 120}), TAG(PT_PLUCK)},
};

static const engine_t ENG_ACID = {
    .name = "ACID",
    .page_title = {"303", "ACCENT"},
    .edit = {
        {"WAVE", N_ACID_WAVE, 0, 3, 0, F_ENUM, 0},
        {"CUTOFF", 0, 0, 127, 40, F_CUTOFF, 0},
        {"RES", 0, 0, 127, 90, F_PCT, 0},
        {"ENV MOD", 0, 0, 127, 70, F_PCT, 0},
        {"DECAY", 0, 0, 127, 60, F_PCT, 0},
        {"ACCENT", 0, 0, 127, 80, F_PCT, 0},
        {"TUNE", 0, -64, 63, 0, F_INT, U_CT},
        {"SLIDE", 0, 0, 127, 45, F_PCT, 0},
    },
    .presets = ACID_PRESETS,
    .npresets = NELEM(ACID_PRESETS),
    .note_on = acid_note_on,
    .render = acid_render,
    .knob = {P_E1, P_E2, P_E3, P_E4},
    .poly = 1,
    .ownenv = 1,
    .done = acid_done,
};
