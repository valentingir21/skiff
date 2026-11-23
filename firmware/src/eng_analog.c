/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* ANALOG: two band-limited oscillators (saw / square / tri / sine / PWM),
 * noise, drive and a trapezoidal low-pass.
 * (Skiff 1.3, from Felucca 1.5) WAVE SYNC and SUB (appended: WAVE 0..4 play as they always did; firmware before reads
 * them as PWM) are two more oscillator pairs (analog_render_x):
 *   SYNC  OSC 2 a saw hard-synced to OSC 1 (a saw): it restarts with OSC 1's every cycle, band-limited at the restart
 *         (polyBLEP scaled by the step the restart makes). DTN (shown SYNC) sets OSC 2 above OSC 1, 3/16 semitone a
 *         step (127: +23.8 semitones), and the SHP routings sweep it (ENV DEST / LFO DEST SHP, the matrix's SHP: 3/16
 *         semitone a step of shape, as PWM takes them for its width): the sync sweep. MIX between the two as for
 *         every pair (OSC 2 alone at 127)
 *   SUB   OSC 1 a saw, OSC 2 a square an octave below it (detuned by DTN in cents as before): MIX (shown SUB) is the
 *         sub oscillator's level */
static const char *const N_ANALOG_WAVE[] = {"SAW", "SQR", "TRI", "SIN", "PWM", "SYNC", "SUB"};
enum { AW_SYNC = 5, AW_SUB = 6 };
static const param_desc_t ANALOG_SYNC = {"SYNC", 0, 0, 127, 10, F_INT, 0};   /* DTN's range and default */
static const param_desc_t ANALOG_SUB = {"SUB", 0, 0, 127, 64, F_PCT, 0};      /* MIX's */

static const param_desc_t *analog_desc(const track_t *t, uint32_t k)
{
    if (k == 1u && t->p[P_E0] == AW_SYNC)
        return &ANALOG_SYNC;
    if (k == 2u && t->p[P_E0] == AW_SUB)
        return &ANALOG_SUB;
    return 0;
}

static void analog_note_on(track_t *t, voice_t *v)
{
    uint32_t sin = t->p[P_E0] == 3;
    if (sin)                                          /* ((2.7, #110) SIN: both oscillators from their zero crossing, */
        v->ph[0] = 0;                                 /* not where the voice's last note left OSC 1 (nor OSC 2 a quarter
                                                       * on: at its peak with MIX up), a click; a retrigger keeps its
                                                       * phases: voice_start) */
    v->ph[1] = sin ? 0u : v->ph[0] + 0x40000000u;
    v->s[0] = v->s[1] = 0;                            /* filter */
    if (!v->s[2])
        v->s[2] = 0x1234567 + (int32_t)v->age;        /* noise state */
    v->s[3] = 0;                                      /* SYNC: no restart pending */
}

/* WAVE SYNC / SUB: OSC 1 a saw, OSC 2 the synced saw / the sub square; then noise, drive and the filter as
 * analog_render. Its own function: analog_render's loop stays what it was */
static __attribute__((noinline)) void analog_render_x(track_t *t, voice_t *v, int32_t *out, uint32_t n,
                                                      const vmod_t *m)
{
    const int16_t *p = t->p;
    uint32_t sub = p[P_E0] == AW_SUB, i;
    int32_t det = p[P_E1], mix = p[P_E2], noise = p[P_E3];
    int32_t cut = (p[P_E4] << 8) + m->cutoff + (p[P_E7] * (v->pitch16 - 60 * 16) >> 4);
    tsvf_t flt;
    int32_t drive = 32768 + p[P_E6] * 512;
    int32_t m2 = mix * 258, m1 = 32767 - m2;
    int32_t nz = noise * 200, drv = p[P_E6];
    uint32_t ph0 = v->ph[0], ph1 = v->ph[1], inc1 = m->inc, inc2;
    int32_t ic1 = v->s[0], ic2 = v->s[1], nst = v->s[2];
    int32_t jl = v->s[3];                             /* SYNC: 1 + the step of a restart a sample ago (16 bits), 0 none */
    if (sub) {                                        /* an octave below OSC 1, DTN as OSC 2 always had it */
        inc2 = (det ? cents_inc(m->pitch16, det, 0) : inc1) >> 1;
    } else {                                          /* DTN and the shape above OSC 1, 3/16 semitone a step */
        inc2 = pitch_inc(clamp(m->pitch16 + det * 3 + (((m->shape - (64 << 8)) * 3) >> 8), 0, 2047));
        if (m->fine)
            inc2 += (uint32_t)((int32_t)(inc2 >> 12) * m->fine);
    }
    tsvf_coef(&flt, cut, p[P_E5]);
    for (i = 0; i < n; i++) {
        int32_t a = osc_saw(ph0, inc1), b, s;
        if (sub) {
            b = osc_pulse(ph1, inc2, 0x80000000u);
        } else {
            int32_t bl = blep(ph1, inc2);
            if (jl) {                                 /* just restarted: the step was jl - 1, not a whole cycle */
                bl = (bl * ((jl - 1) >> 1)) >> 15;
                jl = 0;
            }
            b = (int32_t)(ph1 >> 16) - 32768 - bl;
            if (ph0 > 0xFFFFFFFFu - inc1)             /* OSC 1 restarts it before the next sample: round that step */
                b -= (blep(ph0, inc1) * (int32_t)(ph1 >> 17)) >> 15;
        }
        ph0 += inc1;
        ph1 += inc2;
        if (!sub && ph0 < inc1) {                     /* OSC 1 wrapped: OSC 2 restarts at the same point */
            uint32_t f = ph0 / ((inc1 >> 16) | 1u);   /* the part of a sample since, 0..65535 */
            uint32_t at = ph1 - f * (inc2 >> 16);     /* OSC 2 at the wrap */
            jl = (int32_t)(at >> 16) + 1;
            ph1 = f * (inc2 >> 16);
        }
        s = mulq15(a, m1) + mulq15(b, m2);
        if (nz)
            s += mulq15((int32_t)(noise32(&nst) >> 17) - 16384, nz);
        if (drv)
            s = softclip(((s >> 2) * (drive >> 2)) >> 11);
        s = soft_knee(tsvf_lp(&flt, s >> 1, &ic1, &ic2), 16000) << 1;
        out[i] += voice_amp2(s, m, i);
    }
    v->ph[0] = ph0;
    v->ph[1] = ph1;
    v->s[0] = ic1;
    v->s[1] = ic2;
    v->s[2] = nst;
    v->s[3] = jl;
}

/* WAVE 0..4: the loop as it always was */
static inline __attribute__((always_inline)) void analog_osc(track_t *t, voice_t *v, int32_t *out, uint32_t n,
                                                             const vmod_t *m)
{
    const int16_t *p = t->p;
    uint32_t wave = (uint32_t)p[P_E0], i;
    int32_t det = p[P_E1], mix = p[P_E2], noise = p[P_E3];
    int32_t cut = (p[P_E4] << 8) + m->cutoff + (p[P_E7] * (v->pitch16 - 60 * 16) >> 4);
    tsvf_t flt;
    int32_t drive = 32768 + p[P_E6] * 512;                       /* 1x .. 3x */
    uint32_t inc1 = m->inc;
    uint32_t inc2 = det ? cents_inc(m->pitch16, det, 0) : inc1;      /* DTN in cents */
    uint32_t pw = 0x80000000u + (uint32_t)((m->shape - (64 << 8)) << 15);
    int32_t m2 = mix * 258, m1 = 32767 - m2;                    /* osc mix Q15 */
    int32_t nz = noise * 200, drv = p[P_E6];
    uint32_t ph0 = v->ph[0], ph1 = v->ph[1];                  /* state in locals: out[] may alias v->s[] */
    int32_t ic1 = v->s[0], ic2 = v->s[1], nst = v->s[2];
    tsvf_coef(&flt, cut, p[P_E5]);
    for (i = 0; i < n; i++) {
        int32_t a, b, s;
        switch (wave) {
        case 1:
            a = osc_pulse(ph0, inc1, 0x80000000u);
            b = osc_pulse(ph1, inc2, 0x80000000u);
            break;
        case 2:
            a = osc_tri(ph0);
            b = osc_tri(ph1);
            break;
        case 3:
            a = osc_sine(ph0);
            b = osc_sine(ph1);
            break;
        case 4:
            a = osc_pulse(ph0, inc1, pw);
            b = osc_pulse(ph1, inc2, pw);
            break;
        default:
            a = osc_saw(ph0, inc1);
            b = osc_saw(ph1, inc2);
            break;
        }
        ph0 += inc1;
        ph1 += inc2;
        s = mulq15(a, m1) + mulq15(b, m2);
        if (nz)
            s += mulq15((int32_t)(noise32(&nst) >> 17) - 16384, nz);
        if (drv)
            s = softclip(((s >> 2) * (drive >> 2)) >> 11);   /* pre-shifts: drive is up to 3x, no overflow */
        {   /* filter, linear up to half scale, then a soft knee (only resonance peaks saturate); soft_knee()
             * written out: the host compiler makes this loop 1 % slower with the call */
            int32_t y = tsvf_lp(&flt, s >> 1, &ic1, &ic2);
            int32_t a = y < 0 ? -y : y;
            if (a > 16000) {
                a = 16000 + (softclip((a - 16000) * 2) >> 1);
                y = y < 0 ? -a : a;
            }
            s = y << 1;
        }
        out[i] += voice_amp2(s, m, i);
    }
    v->ph[0] = ph0;
    v->ph[1] = ph1;
    v->s[0] = ic1;
    v->s[1] = ic2;
    v->s[2] = nst;
}
static void analog_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    if (t->p[P_E0] >= AW_SYNC)
        analog_render_x(t, v, out, n, m);
    else
        analog_osc(t, v, out, n, m);
}

static const preset_t ANALOG_PRESETS[] = {
    {"SAW LEAD", {0, 12, 64, 0, 90, 30, 10, 64}, {4, 70, 100, 50}, 20, 1, FX(0, 10, 45, 30), TAG(PT_LEAD)},
    {"SOFT PAD", {0, 20, 64, 4, 60, 10, 0, 32}, {80, 90, 110, 95}, 10, 0, FX(0, 60, 20, 70), TAG(PT_PAD)},
    {"SQR BASS", {1, 0, 0, 0, 50, 70, 40, 64}, {0, 60, 40, 30}, 30, 1, FX(5, 0, 10, 10), TAG(PT_BASS)},
    {"PWM STR", {4, 8, 40, 0, 75, 20, 0, 48}, {60, 80, 110, 85}, 8, 0, FX(0, 50, 20, 60), TAG(PT_PAD)},
    {"ACID", {0, 0, 0, 0, 50, 100, 25, 64}, {0, 55, 20, 30}, 48, 1, FX(20, 0, 45, 15), TAG(PT_BASS)},
    {"LD HOOVER", {4, 28, 64, 0, 96, 18, 30, 64}, {2, 70, 110, 50}, 6, 1, FX(20, 100, 25, 45), MORE({P_VOICE, V_UNISON}, {P_M1SRC, 9}, {P_M1DST, 1}, {P_M1AMT, -30}), TAG(PT_LEAD)},
    {"RAVE", {4, 30, 64, 0, 85, 20, 30, 64}, {20, 80, 110, 60}, 10, 1, FX(30, 40, 30, 30), TAG(PT_LEAD)},
    {"SUB BASS", {3, 0, 0, 0, 40, 0, 20, 0}, {0, 60, 100, 20}, 0, 1, FX(0, 0, 0, 10), TAG(PT_BASS)},
    {"SYNTH PLUCK", {0, 8, 50, 0, 30, 40, 0, 64}, {0, 88, 0, 60}, 55, 0, FX(0, 20, 50, 30), TAG(PT_PLUCK)},
    {"SYNTH BRASS", {0, 10, 64, 0, 45, 20, 10, 64}, {35, 70, 90, 45}, 40, 0, FX(0, 20, 20, 40), TAG(PT_LEAD)},
    {"WIND SWEEP", {0, 0, 0, 90, 30, 90, 0, 0}, {60, 90, 60, 80}, 50, 0, FX(0, 30, 30, 70), TAG(PT_FX)},
    {"STRINGS", {4, 6, 64, 0, 92, 8, 0, 48}, {48, 90, 118, 72}, 6, 0, FX(0, 95, 15, 55), MORE({P_LD_SHP, 20}, {P_LRATE, 72}, {P_LD_PIT, 8}, {P_LFADE, 80}), TAG(PT_PAD)},   /* ((Skiff phase 9) as SOFT PAD to the ear: a bright string
                                                                         * machine now, its PWM moving, a late vibrato) */
    /* (Skiff) for the themes: CRYPT (dungeon synth), NEON (synthwave), QUARK (particles) */
    {"BS ACID 303", {0, 0, 0, 0, 56, 112, 30, 64}, {0, 80, 110, 20}, 40, 1, FX(25, 0, 25, 6), MORE({P_M1SRC, 9}, {P_M1DST, 2}, {P_M1AMT, 63}, {P_GLIDE, 40}), TAG(PT_BASS)},
    {"WAR HORN", {0, 5, 64, 4, 42, 12, 20, 64}, {72, 80, 110, 80}, 30, 1, FX(0, 10, 10, 90), MORE({P_LRATE, 80}, {P_LFADE, 96}, {P_LD_PIT, 8}), TAG(PT_LEAD)},
    {"DARK STRINGS", {1, 4, 64, 6, 40, 34, 22, 40}, {100, 90, 120, 105}, 10, 0, FX(0, 30, 0, 110), MORE({P_LRATE, 30}, {P_LD_FLT, 8}, {P_LD_PIT, 8}, {P_LFADE, 110}), TAG(PT_PAD)},   /* ((phase 9) hollow and dark, a slow swell) */
    {"NIGHT BASS", {0, 7, 64, 0, 40, 35, 15, 64}, {0, 64, 50, 30}, 45, 1, FX(0, 5, 10, 8), TAG(PT_BASS)},
    {"NEON LEAD", {0, 14, 64, 0, 92, 25, 12, 64}, {6, 70, 110, 55}, 14, 1, FX(0, 25, 55, 40), MORE({P_LRATE, 86}, {P_LFADE, 92}, {P_LD_PIT, 11}, {P_GLIDE, 28}), TAG(PT_LEAD)},
    {"SUNSET PAD", {4, 9, 55, 0, 74, 15, 0, 48}, {78, 90, 115, 92}, 10, 0, FX(0, 85, 25, 70), MORE({P_LWAVE, 1}, {P_LRATE, 52}, {P_LD_SHP, 48}), TAG(PT_PAD)},
    {"ST TECHNO", {0, 7, 64, 0, 66, 40, 20, 64}, {0, 66, 0, 48}, 34, 0, FX(15, 20, 55, 55), TAG(PT_KEYS)},
    {"SYN TOM", {3, 0, 0, 6, 127, 0, 25, 0}, {0, 76, 0, 60}, 0, 0, FX(0, 0, 10, 45), MORE({P_ED_PIT, 63}), TAG(PT_DRUM)},
    {"QUANTA", {3, 0, 0, 0, 127, 0, 0, 0}, {0, 64, 127, 50}, 0, 1, FX(0, 0, 55, 35), MORE({P_LWAVE, 4}, {P_LRATE, 96}, {P_LD_PIT, 50}), TAG(PT_FX)},
    /* (Skiff phase 9) more for every engine: basses, kits */
    {"REESE", {0, 40, 64, 0, 52, 15, 30, 32}, {0, 64, 127, 30}, 10, 1, FX(10, 20, 5, 10), MORE({P_LRATE, 30}, {P_LD_FLT, 8}), TAG(PT_BASS)},
    {"DRIVE BASS", {0, 14, 64, 0, 62, 20, 10, 64}, {0, 64, 64, 25}, 24, 1, FX(40, 0, 5, 5), MORE({P_DTYPE, 1}, {P_DTONE, -32}), TAG(PT_BASS)},
    /* (1.3) WAVE SYNC: the envelope sweeps the synced saw (ENV DEST SHAPE); SUB: a saw on its square an octave down */
    {"HARD SYNC", {5, 36, 96, 0, 88, 24, 12, 64}, {2, 70, 96, 40}, 16, 1, FX(0, 15, 40, 30), MORE({P_ED_SHP, 44}, {P_GLIDE, 20}), TAG(PT_LEAD)},
    {"SUB SAW", {6, 0, 80, 0, 70, 22, 6, 64}, {0, 60, 110, 22}, 20, 1, FX(0, 0, 5, 10), TAG(PT_BASS)},    /* (techno-presets) a techno bank: LD leads, BS basses, PA pads, ST stabs, DR drones, FX, KT kits (PRESETS.md) */
    {"LD SSAW UNI", {0, 14, 64, 0, 100, 12, 8, 64}, {2, 70, 115, 55}, 6, 1, FX(0, 25, 40, 50), MORE({P_VOICE, V_UNISON}, {P_DETUNE, 72}, {P_SPRD, 110}), TAG(PT_LEAD)},
    {"LD TRANCE", {0, 9, 64, 0, 108, 16, 4, 72}, {4, 74, 110, 60}, 10, 1, FX(0, 35, 55, 60), MORE({P_VOICE, V_UNISON}, {P_DETUNE, 54}, {P_SPRD, 100}, {P_GLIDE, 16}), TAG(PT_LEAD)},
    {"LD HARDTECH", {6, 8, 70, 0, 90, 30, 70, 64}, {0, 70, 110, 35}, 18, 1, FX(70, 0, 20, 20), MORE({P_DTYPE, 1}, {P_DTONE, 12}, {P_GLIDE, 14}), TAG(PT_LEAD)},
    {"LD GABBER", {4, 34, 64, 0, 88, 24, 70, 64}, {0, 70, 110, 40}, 10, 1, FX(80, 70, 15, 25), MORE({P_VOICE, V_UNISON}, {P_M1SRC, 9}, {P_M1DST, 1}, {P_M1AMT, -40}), TAG(PT_LEAD)},
    {"LD SYNC", {5, 40, 100, 0, 96, 22, 30, 64}, {0, 72, 105, 40}, 8, 1, FX(30, 15, 40, 30), MORE({P_ED_SHP, 50}, {P_DTYPE, 1}, {P_GLIDE, 18}), TAG(PT_LEAD)},
    {"LD PLUCK", {0, 12, 64, 0, 40, 45, 10, 64}, {0, 70, 0, 52}, 52, 0, FX(0, 25, 65, 45), TAG(PT_PLUCK)},
    {"LD SSAW POLY", {0, 22, 64, 0, 100, 14, 6, 64}, {3, 72, 112, 58}, 6, 0, FX(0, 110, 40, 55), MORE({P_SPRD, 120}), TAG(PT_LEAD)},
    {"LD ACID", {0, 3, 30, 0, 50, 100, 45, 80}, {0, 70, 60, 30}, 44, 1, FX(40, 0, 50, 25), MORE({P_GLIDE, 30}, {P_DTYPE, 1}), TAG(PT_LEAD)},
    {"BS ACID SQR", {1, 0, 0, 0, 42, 106, 30, 64}, {0, 64, 25, 20}, 54, 1, FX(20, 0, 30, 6), MORE({P_GLIDE, 40}), TAG(PT_BASS)},
    {"BS ACID HARD", {0, 0, 0, 0, 52, 112, 80, 64}, {0, 60, 20, 20}, 56, 1, FX(75, 0, 20, 5), MORE({P_DTYPE, 1}, {P_DTONE, 16}, {P_GLIDE, 40}), TAG(PT_BASS)},
    {"BS RUMBLE", {3, 0, 0, 0, 36, 0, 70, 0}, {0, 84, 60, 72}, 0, 1, FX(45, 0, 35, 85), MORE({P_DTONE, -40}), TAG(PT_BASS)},
    {"BS REESE DRK", {0, 30, 64, 0, 56, 20, 40, 40}, {0, 64, 127, 30}, 6, 1, FX(35, 15, 0, 8), MORE({P_LRATE, 24}, {P_LD_FLT, 10}, {P_DTYPE, 4}), TAG(PT_BASS)},
    {"BS SUB CLEAN", {2, 0, 0, 0, 60, 0, 12, 0}, {0, 64, 127, 18}, 0, 1, FX(0, 0, 0, 0), TAG(PT_BASS)},
    {"BS HARDTECH", {6, 0, 50, 0, 58, 40, 60, 64}, {0, 60, 40, 20}, 40, 1, FX(60, 0, 10, 6), MORE({P_DTYPE, 1}, {P_DTONE, 8}), TAG(PT_BASS)},
    {"BS FUZZ", {1, 6, 64, 0, 62, 30, 50, 64}, {0, 64, 100, 22}, 20, 1, FX(85, 0, 5, 5), MORE({P_DTYPE, 4}, {P_DTONE, -16}), TAG(PT_BASS)},
    {"BS ROLLING", {0, 8, 64, 0, 52, 35, 20, 64}, {0, 58, 30, 20}, 40, 1, FX(20, 0, 15, 5), TAG(PT_BASS)},
    {"BS MINI", {0, 6, 40, 0, 42, 55, 35, 64}, {0, 68, 70, 28}, 36, 1, FX(15, 0, 10, 8), MORE({P_GLIDE, 22}), TAG(PT_BASS)},
    {"PA DARK", {4, 10, 64, 6, 44, 20, 0, 40}, {96, 92, 115, 100}, 8, 0, FX(0, 70, 25, 105), MORE({P_LRATE, 26}, {P_LD_FLT, 12}, {P_LD_SHP, 30}, {P_SPRD, 100}), TAG(PT_PAD)},
    {"PA SSAW", {0, 24, 64, 0, 80, 10, 0, 50}, {70, 90, 118, 96}, 4, 0, FX(0, 115, 30, 95), MORE({P_SPRD, 127}, {P_LRATE, 40}, {P_LD_PIT, 4}), TAG(PT_PAD)},
    {"PA TRANCE", {0, 16, 64, 0, 96, 18, 0, 56}, {60, 88, 115, 90}, 12, 0, FX(0, 95, 45, 85), MORE({P_SPRD, 110}), TAG(PT_PAD)},
    {"ST RAVE", {4, 20, 64, 0, 80, 20, 25, 64}, {0, 64, 10, 45}, 24, 0, FX(20, 70, 50, 50), MORE({P_SPRD, 80}), TAG(PT_KEYS)},
    {"ST SSAW", {0, 16, 64, 0, 92, 18, 12, 64}, {0, 68, 0, 50}, 26, 1, FX(10, 30, 55, 55), MORE({P_VOICE, V_UNISON}, {P_DETUNE, 70}, {P_SPRD, 115}), TAG(PT_KEYS)},
    {"ST CHORD", {0, 20, 64, 0, 72, 34, 18, 64}, {0, 66, 0, 52}, 34, 0, FX(15, 90, 55, 60), MORE({P_SPRD, 110}), TAG(PT_KEYS)},
    {"FX IMPACT", {3, 0, 0, 70, 80, 10, 90, 0}, {0, 96, 0, 96}, 0, 0, FX(60, 0, 25, 115), MORE({P_ED_PIT, 63}), TAG(PT_FX)},
    {"FX ZAP", {3, 0, 0, 0, 127, 0, 20, 0}, {0, 70, 0, 50}, 0, 0, FX(20, 0, 60, 40), MORE({P_ED_PIT, 63}, {P_M1SRC, 9}, {P_M1DST, 1}, {P_M1AMT, 63}), TAG(PT_FX)},
    {"DR DARK", {0, 30, 64, 20, 34, 40, 20, 30}, {110, 100, 127, 110}, 0, 0, FX(20, 60, 30, 120), MORE({P_LRATE, 14}, {P_LD_FLT, 20}, {P_SPRD, 100}), TAG(PT_PAD)},
};

static const engine_t ENG_ANALOG = {
    .name = "ANALOG",
    .page_title = {"OSC", "FLT"},
    .edit = {
        {"WAVE", N_ANALOG_WAVE, 0, 6, 0, F_ENUM, 0},   /* ((1.3) SYNC SUB appended) */
        {"DETUNE", 0, 0, 127, 10, F_INT, U_CT},
        {"MIX", 0, 0, 127, 64, F_PCT, 0},
        {"NOISE", 0, 0, 127, 0, F_PCT, 0},
        {"CUTOFF", 0, 0, 127, 90, F_CUTOFF, 0},
        {"RES", 0, 0, 127, 30, F_PCT, 0},
        {"DRIVE", 0, 0, 127, 0, F_PCT, 0},
        {"KEYTRK", 0, 0, 127, 64, F_PCT, 0},
    },
    .presets = ANALOG_PRESETS,
    .npresets = NELEM(ANALOG_PRESETS),
    .note_on = analog_note_on,
    .render = analog_render,
    .desc = analog_desc,
    .knob = {P_E4, P_E5, P_ATK, P_REL},
    .keep = 0x03,                /* the filter */
};
