/* SPDX-License-Identifier: GPL-3.0-only */
/* (Gir, rumble) the hard-techno RUMBLE of DRUM's and DRUM-X's KICK: the kick alone into a small reverb of its own, as a
 * producer sends it to a reverb bus (the shared REVERB cannot be filtered or driven for the kick only):
 *   the kick -> a reverb (3 combs + an allpass) -> (R.TUNE) a comb at the note -> a resonant low-pass (R.TONE)
 *            -> a soft clip (R.DRIVE) -> ducked under each kick (R.DUCK) -> RUMBLE its level, added to the kick lane.
 * Everything below 300 Hz runs at 1/24 of the rate (1837 Hz: 1/24 the work and the buffers), the input the mean of 24;
 * the clip and the duck at a quarter of the rate (11 kHz: the clip's harmonics), interpolated back up. R.TUNE OFF: the
 * reverb as it is; a note: the comb at its period (E1 .. D#2, A = 55 Hz), the tail at that pitch. The duck is its own,
 * inside the engine: the gain falls to 0 in ~2 ms at each kick and comes back over 20 .. 600 ms (squared: a pump),
 * whatever the track's FX > DUCK. RUMBLE 0: nothing runs (the kit as before, bit for bit).
 * State: rb_t after the 8 lanes in a heavy part's engine area (engines.c), a light part's in rb_lite; from silence at
 * the engine switch and when RUMBLE turns on. The kick lane's voice stays on while the tail sounds (drum_amp,
 * drumx_amp: rb_keep). */
#define RB_D 24u                     /* the decimation */
#define RB_Q 4u                      /* the clip's: a quarter of the rate */
#define RB_AP 15u                    /* the allpass's length */
#define RB_RN 48u                    /* the tuned comb's room: the longest period (E1: 44.6) and its interpolation */
static const uint8_t RB_LEN[5] = {59, 73, 89, RB_AP, RB_RN};   /* the combs' (prime: 32 40 48 ms at 1837 Hz), .. */
#define RB_NBUF (59u + 73u + 89u + RB_AP + RB_RN)
typedef struct {
    int16_t b[RB_NBUF];              /* the three combs, the allpass, the tuned comb, one after the other */
    int32_t acc, d[3], low, band, y0, y1, g1, dk, post, cur, inc, corr, hp;
    int32_t fb, f, da, tune;         /* this block's comb feedback, low-pass coefficient, duck rate, note (rb_run) */
    uint8_t ix[5], k, atk, on, hold, hit, neg;
} rb_t;
#define RB_OFS ((sizeof(drum_lane_t) * DV_NLANE + 7u) & ~7u)   /* where it sits in a heavy area (after DRUM's lanes) */

/* the tuned comb's period at each note (C .. B, Q8 samples of 1837 Hz): C .. D# in octave 2, E .. B in octave 1 */
static const uint16_t RB_PER[12] = {7192, 6788, 6407, 6048, 11417, 10776, 10171, 9600, 9061, 8553, 8073, 7620};

static inline int32_t rb_mul(int32_t x, int32_t g)  /* x * g (Q15), toward 0: a feedback loop dies out, no -1 left */
{
    int32_t v = (x * g) >> 15;
    return v < 0 ? v + 1 : v;
}
static inline int16_t rb_s16(int32_t x) { return (int16_t)clamp(x, -32767, 32767); }

/* a heavy part's in its area, after the lanes (no RAM of its own: PHYS's slots are bigger); a light part's (its area
 * no bigger than DRUM's lanes) here, in .bss (the pool keeps its 8 KB spare) */
static rb_t rb_lite[NTRK - NHEAVY];
static __attribute__((noinline)) rb_t *rb_of(const track_t *t)
{
    uint32_t k = (uint32_t)(t - trk) % NPART;
    return k >= NHEAVY ? &rb_lite[k - NHEAVY] : (rb_t *)((uint8_t *)eng_part(t) + RB_OFS);
}

/* a kick struck: the duck falls (its attack, ~2 ms) */
static void rb_hit(const track_t *t)
{
    rb_of(t)->hit = 1;
}

/* the kick lane's voice lives on while the tail sounds (its render renews hold; each block counts it down, so a voice
 * no longer rendered ends) */
static int rb_keep(track_t *t)
{
    rb_t *r = rb_of(t);
    if (t->p[P_RB_MIX] <= 0 || !r->on || !r->hold)
        return 0;
    r->hold--;
    return 1;
}

/* one sample at 1837 Hz: x the kick, decimated -> the rumble before its clip (r->y1), the duck's gain (r->g1) */
static __attribute__((noinline)) void rb_step(rb_t *r, int32_t x)
{
    int16_t *b = r->b;
    int32_t o[3], s = 0, a;
    uint32_t c;
    r->hp += ((x << 8) - r->hp) >> 4;                  /* the kick's DC out (~18 Hz: the combs would raise it x14) */
    x -= r->hp >> 8;
    for (c = 0; c < 3u; b += RB_LEN[c], c++) {         /* three combs, each loop damped (a one-pole, half way) */
        o[c] = b[r->ix[c]];
        r->d[c] += (o[c] - r->d[c]) >> 1;
        b[r->ix[c]] = rb_s16(x + rb_mul(r->d[c], r->fb));
        s += o[c];
    }
    s = (s * 10923) >> 15;                             /* their mean */
    a = b[r->ix[3]];                                   /* an allpass (g 0.6): the echoes spread */
    b[r->ix[3]] = rb_s16(s + rb_mul(a, 19661));
    s = a - rb_mul(s + rb_mul(a, 19661), 19661);
    b += RB_AP;
    if (r->tune >= 0) {                                /* R.TUNE: a comb at the note's period, its harmonics ring */
        uint32_t per = RB_PER[r->tune % 12], j0 = r->ix[4] + RB_RN - (per >> 8), j1;
        int32_t gt = (r->fb + 32767) >> 1, rc;
        j0 = j0 >= RB_RN ? j0 - RB_RN : j0;
        j1 = j0 ? j0 - 1u : RB_RN - 1u;
        rc = b[j0] + (((b[j1] - b[j0]) * (int32_t)(per & 255u)) >> 8);
        gt = gt > 31785 ? 31785 : gt;                  /* (0.97 at most) */
        b[r->ix[4]] = rb_s16(rb_mul(s, (32768 - gt) * 2) + rb_mul(rc, gt));
        s = (s >> 2) + rc;
    }
    for (c = 0; c < 5u; c++)
        r->ix[c] = (uint8_t)(r->ix[c] + 1u == RB_LEN[c] ? 0u : r->ix[c] + 1u);
    r->low = clamp(r->low + ((r->f * r->band) >> 15), -65535, 65535);   /* R.TONE: a state-variable low-pass, Q ~1.7 */
    a = clamp(s - r->low - ((19661 * r->band) >> 15), -32767, 32767);   /* (f < 33700, each term < 2^16: < 2^31) */
    r->band = clamp(r->band + ((r->f * a) >> 15), -32767, 32767);
    r->corr = clamp(r->corr + (((x >> 4) * (r->low >> 4)) >> 6), -(1 << 30), 1 << 30);   /* (its phase: below) */
    r->y0 = r->y1;
    r->y1 = r->neg ? -r->low : r->low;
    if (r->y1 > 24 || r->y1 < -24)                     /* still sounding: the voice stays (~45 ms after it ends) */
        r->hold = 64;
    if (r->hit) {                                      /* the duck: down at a kick, back by R.DUCK's time */
        r->hit = 0;
        r->atk = r->da ? 4 : 0;
    }
    if (r->atk) {
        r->atk--;
        r->dk >>= 1;
        if (!r->atk) {                                 /* ducked to -48 dB: its phase, the one that adds to the kick's */
            if (r->corr < -(1 << 12) || r->corr > 1 << 12)   /* tail (their correlation over the beat before, the */
                r->neg = r->corr < 0;                  /* rumble's own phase), turned here unheard (a long kick tail */
            r->corr = 0;                               /* and a rumble at its pitch in opposite phase would hollow the
                                                        * kick, not fill it) */
        }
    } else {
        r->dk += (((1 << 30) - r->dk) >> 15) * r->da;
    }
    a = r->dk >> 15;
    r->g1 = (a * a) >> 15;                             /* (squared: slow first, the pump) */
}

/* the kick lane's block x (n) -> its rumble into o (n). The kick's own level and ramp come after (the engine's
 * voice_amp2), so the rumble follows the lane's LEVEL and PAN */
static __attribute__((noinline)) void rb_run(track_t *t, const int32_t *x, int32_t *o, uint32_t n)
{
    const int16_t *p = t->p;
    rb_t *r = rb_of(t);
    int32_t mix = p[P_RB_MIX], drv = p[P_RB_DRIVE], tn = p[P_RB_TONE], dec = 127 - p[P_RB_DECAY], du = p[P_RB_DUCK];
    int32_t lv, gd, mk = 0, fc, acc, cur, inc;
    uint32_t i = 0, k;
    if (!r->on) {                                      /* turned on: from silence */
        memset(r, 0, sizeof *r);
        r->dk = 1 << 30;
        r->g1 = 32767;
        r->on = 1;
    }
    lv = mix * mix * 3;                                /* RUMBLE: square law, x3 at 127 (Q14, below) */
    fc = 40 + tn * tn * 260 / 16129;                   /* R.TONE: 40 .. 300 Hz */
    r->f = fc * 112;                                   /* (2 sin(pi fc / fs), Q15, at 1837 Hz: 300 Hz 2.7 % over) */
    r->fb = 30474 - dec * dec * 17 / 16;               /* R.DECAY: the combs' feedback 0.41 .. 0.93 (~0.3 .. ~3 s) */
    r->tune = p[P_RB_TUNE];
    r->da = du ? 32768 / (37 + du * du * 1066 / 16129) : 0;   /* R.DUCK: back in 20 .. 600 ms, 0 none */
    gd = 4096 + drv * 15 * 4096 / 127;                 /* R.DRIVE: x1 .. x16 into the clip, the level kept (Q12) */
    if (drv > 0)
        mk = (int32_t)((19661u << 15) / (uint32_t)softclip((19661 * gd) >> 12));
    fc *= 4 * 75;                                      /* the clip's harmonics: a one-pole at ~4x R.TONE (11 kHz) */
    acc = r->acc;
    cur = r->cur;
    inc = r->inc;
    k = r->k;
    while (i < n) {                                    /* runs to the next quarter point: the input summed, the */
        uint32_t m = RB_Q - (k & (RB_Q - 1u)), j;      /* output a line to the next value */
        m = m < n - i ? m : n - i;
        for (j = 0; j < m; j++) {
            acc += x[i + j];
            o[i + j] = cur += inc;
        }
        i += m;
        k += m;
        if (k & (RB_Q - 1u))
            break;                                     /* (the block ended between two) */
        if (k == RB_D) {
            k = 0;
            rb_step(r, clamp((acc * 341) >> 15, -32767, 32767));   /* (the mean of 24, at a quarter) */
            acc = 0;
        }
        {                                              /* the next quarter point's value: interpolated, clipped, */
            int32_t s = r->y0 + (((r->y1 - r->y0) * (int32_t)k) / (int32_t)RB_D);   /* smoothed, ducked */
            s = clamp(s * 4, -32000, 32000);            /* (x16 of it < 2^31) */
            if (mk)
                s = (softclip((s * gd) >> 12) * mk) >> 15;
            r->post += ((s - r->post) * fc) >> 15;
            s = (r->post * ((r->g1 * (lv >> 1)) >> 14)) >> 14;   /* (|post| < 37000, the gain < 48400: < 2^31) */
            inc = (s - cur) / (int32_t)RB_Q;
        }
    }
    r->acc = acc;
    r->cur = cur;
    r->inc = inc;
    r->k = (uint8_t)k;
}
