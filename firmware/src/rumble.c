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
    int32_t acc, d[3], low, band, y0, y1, g0, g1, gc0, gc1, dk, pre1, post, post2, cur, inc, corr, hp;
    int32_t fb, f, da, tune;         /* this block's comb feedback, low-pass coefficient, duck rate, note (rb_run) */
    uint32_t tpos;                   /* samples since the last kick (R.SLICE's grid, R.ATTACK's ramp) */
    uint16_t pre, bad;               /* the sound it was set up for (a kit load clears it); the watchdog's count */
    uint8_t ix[5], k, atk, on, hold, hit, neg, play, fade;
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

static uint8_t rb_play_gen;                            /* (each PLAY: seq.c seq_start, midi_clock.c's START; a rumble
                                                        * not heard while stopped still sees it) */
/* a kick struck: the duck falls (its attack, ~2 ms), R.SLICE's grid and R.ATTACK's ramp start again */
static void rb_hit(const track_t *t)
{
    rb_t *r = rb_of(t);
    r->hit = 1;
    r->tpos = 0;
}
static uint32_t beat_samples(void);                   /* (fx.c: the quarter note, the song's tempo or the clock's) */

/* (Gir 1.0) the rumble out of the kick lane, at its level (the lane's LEVEL, the hit's ramp), into its track's own
 * buffer: fx.c adds it to the track after FX > DRIVE (rb_mix), so the track's drive does not grind a sustained low
 * into a fizz of high harmonics (the metallic noise heard with DRIVE up); its own R.DRIVE is its saturation */
static int32_t rb_buf[NPART][CTL];
static uint8_t rb_have[NPART];                         /* (a byte a track: the two cores write their own) */
static __attribute__((noinline)) void rb_out(const track_t *t, const int32_t *x, const vmod_t *m, uint32_t n)
{
    uint32_t k = (uint32_t)(t - trk) % NPART, i;
    for (i = 0; i < n; i++)
        rb_buf[k][i] += voice_amp2(x[i], m, i);
    rb_have[k] = 1;
}
static __attribute__((noinline)) void rb_mix(const track_t *t, int32_t *b, uint32_t n)
{
    uint32_t k = (uint32_t)(t - trk) % NPART, i;
    if (!rb_have[k])                                   /* (a track without: back at once) */
        return;
    for (i = 0; i < n; i++) {
        b[i] += rb_buf[k][i];
        rb_buf[k][i] = 0;
    }
    rb_have[k] = 0;
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
    r->bad = (uint16_t)(r->low >= 65535 || r->low <= -65535 || r->band >= 32767 || r->band <= -32767 ? r->bad + 1u : 0u);
    if (r->bad > 1837u && !r->fade)                    /* (the watchdog: a second held at its bounds, never in use; */
        r->fade = 32;                                  /* faded out and cleared as a kit load is) */
    if (r->y1 > 24 || r->y1 < -24)                     /* still sounding: the voice stays (~45 ms after it ends) */
        r->hold = 64;
    if (r->hit) {                                      /* the duck: down at a kick, back by R.DUCK's time */
        r->hit = 0;
        r->atk = r->da ? 10 : 0;
    }
    if (r->atk) {
        r->atk--;
        r->dk -= r->dk >> 2;                           /* (x0.75 a step: -50 dB in 5.4 ms, no click on a soft kick) */
        if (!r->atk) {                                 /* ducked to -50 dB: its phase, the one that adds to the kick's */
            if (r->corr < -(1 << 12) || r->corr > 1 << 12)   /* tail (their correlation over the beat before, the */
                r->neg = r->corr < 0;                  /* rumble's own phase), turned here unheard (a long kick tail */
            r->corr = 0;                               /* and a rumble at its pitch in opposite phase would hollow the
                                                        * kick, not fill it) */
        }
    } else {
        r->dk += (((1 << 30) - r->dk) >> 15) * r->da;
    }
    a = r->dk >> 15;
    r->g1 = (a * a) >> 15;                             /* (squared: slow first, the pump; drawn as a line between the
                                                        * steps, as the signal: steps at 1837 Hz were inharmonic lines) */
}

/* R.SLICE: the beat's 16ths (0 the kick's) that carry a pulse, each bit a step; CLASSIC none (the tail as it is) */
static const uint8_t RB_SLICE[5] = {0, 4u, 2u | 8u, 2u | 4u | 8u, 2u | 4u | 8u};   /* CLASSIC OFFBEAT DOUBLE ROLL GATE */

/* R.SLICE and R.ATTACK at tpos (samples since the kick) -> the rumble's gain, Q15. q: the quarter note, sw: the swing's
 * shift of the odd 16ths, a: R.ATTACK's ramp (samples, CLASSIC) or its share of a pulse (/128, the others) */
static __attribute__((noinline)) int32_t rb_shape(uint32_t tpos, uint32_t q, int32_t sw, uint32_t mode, uint32_t a)
{
    uint32_t st = q / 4u, beg, end, len, up, dn, j;
    if (!mode)                                         /* CLASSIC: one ramp up from the kick */
        return tpos >= a ? 32767 : (int32_t)((tpos * ((32767u << 8) / a)) >> 8);
    tpos %= q;                                         /* (between kicks further apart: the grid goes on) */
    for (j = 1; j < 4u; j++) {
        beg = j * st + (uint32_t)((j & 1u) ? sw : 0);  /* (the swing delays the odd 16ths, as the sequencer's) */
        end = j < 3u ? (j + 1u) * st + (uint32_t)(((j + 1u) & 1u) ? sw : 0) : q;
        if (!((RB_SLICE[mode] >> j) & 1u) || tpos < beg || tpos >= end)
            continue;
        len = (end - beg) * (mode == 4u ? 4u : 7u) / 8u;   /* (GATE: half a 16th; the others: 7/8 of it) */
        up = len > 88u ? 44u + (len - 44u) * a / 160u : len / 2u + 1u;   /* R.ATTACK: its rise, 1 ms .. 80 % of it */
        dn = len / 6u + 1u;                            /* (its fall: a sixth, 2 ms at least at 150 BPM: no click) */
        tpos -= beg;
        if (tpos >= len)
            return 0;
        if (tpos < up)
            return (int32_t)((tpos * ((32767u << 8) / up)) >> 8);
        if (tpos > len - dn)
            return (int32_t)(((len - tpos) * ((32767u << 8) / dn)) >> 8);
        return 32767;
    }
    return 0;
}

/* from silence: the whole state cleared (keep: the grid, the duck and the voice's hold kept, a clear while it plays) */
static __attribute__((noinline)) void rb_clear(rb_t *r, uint16_t pre, int keep)
{
    uint32_t tp = r->tpos;
    int32_t dk = keep ? r->dk : 1 << 30, a = dk >> 15;
    uint8_t at = r->atk, h = r->hold, k = r->k;
    int32_t cur = r->cur, inc = r->inc;
    memset(r, 0, sizeof *r);
    if (keep) {
        r->atk = at; r->hold = h; r->tpos = tp; r->k = k; r->cur = cur; r->inc = inc;   /* (the line down to 0 ends) */
    }
    r->dk = dk;
    r->g0 = r->g1 = r->gc0 = r->gc1 = (a * a) >> 15;
    r->on = 1;
    r->pre = pre;
    r->play = rb_play_gen;
}

/* the kick lane's block x (n) -> its rumble into o (n). The kick's own level and ramp come after (the engine's
 * voice_amp2), so the rumble follows the lane's LEVEL and PAN */
static __attribute__((noinline)) void rb_run(track_t *t, const int32_t *x, int32_t *o, uint32_t n)
{
    const int16_t *p = t->p;
    rb_t *r = rb_of(t);
    int32_t mix = p[P_RB_MIX], drv = p[P_RB_DRIVE], tn = p[P_RB_TONE], dec = 127 - p[P_RB_DECAY], du = p[P_RB_DUCK];
    int32_t lv, gd, mk = 0, fc, acc, cur, inc, sw;
    uint32_t i = 0, k, q = beat_samples(), mode = (uint32_t)clamp(p[P_RB_SLICE], 0, 4), at = (uint32_t)clamp(p[P_RB_ATK], 0, 127);
    uint16_t pre = (uint16_t)(t->preset | (uint32_t)t->engine << 8);
    if (!r->on)                                        /* turned on: from silence */
        rb_clear(r, pre, 0);
    if ((r->pre != pre || r->play != rb_play_gen) && !r->fade)   /* another kit, or a PLAY: from silence (a 3 ms */
        r->fade = 32;                                  /* fade, then cleared: no state lives on into it) */
    r->pre = pre;
    r->play = rb_play_gen;
    lv = mix * mix * 3;                                /* RUMBLE: square law, x3 at 127 (Q14, below) */
    fc = 40 + tn * tn * 260 / 16129;                   /* R.TONE: 40 .. 300 Hz */
    r->f = fc * 112;                                   /* (2 sin(pi fc / fs), Q15, at 1837 Hz: 300 Hz 2.7 % over; the
                                                        * filter stays stable to f 1.6 with its damping) */
    r->fb = 30474 - dec * dec * 17 / 16;               /* R.DECAY: the combs' feedback 0.41 .. 0.93 (~0.3 .. ~3 s) */
    r->tune = p[P_RB_TUNE];
    r->da = du ? 32768 / (37 + du * du * 1066 / 16129) : 0;   /* R.DUCK: back in 20 .. 600 ms, 0 none */
    gd = 4096 + drv * 15 * 4096 / 127;                 /* R.DRIVE: x1 .. x16 into the clip, the level kept (Q12) */
    if (drv > 0)
        mk = (int32_t)((19661u << 15) / (uint32_t)softclip((19661 * gd) >> 12));
    fc = (fc * 3 > 400 ? 400 : fc * 3) * 19;           /* the clip's harmonics out: two one-poles at 3x R.TONE, 400 Hz at
                                                        * most, at 11 kHz (2 pi fc / 11025, Q15: 0.13 .. 0.23). ((Gir
                                                        * 1.0 RC2) one pole, x300: past 2 above R.TONE 105, unstable,
                                                        * the tail froze into a noise; and its highs a metallic fizz) */
    sw = track_swing(t) * (int32_t)(q / 4u) / 250;     /* (core.h swing_step_len: the odd 16ths' shift) */
    at = mode ? at : at * at * 13230u / 16129u + 1u;   /* R.ATTACK: CLASSIC's ramp 0 .. 300 ms, a pulse's share */
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
        r->tpos += m;
        if (k & (RB_Q - 1u))
            break;                                     /* (the block ended between two) */
        if (k == RB_D) {
            k = 0;
            rb_step(r, clamp((acc * 341) >> 15, -32767, 32767));   /* (the mean of 24, at a quarter) */
            acc = 0;
            if ((r->post2 >= 40000 || r->post2 <= -40000) && r->bad < 60000u)   /* (the watchdog: held at its bound) */
                r->bad += 64u;
            r->gc0 = r->gc1;                           /* the duck, R.SLICE and R.ATTACK in one gain, at 1837 Hz (their
                                                        * ramps 1 ms at least), drawn as a line to the next step */
            r->gc1 = mode || at > 1u ? (r->g1 * rb_shape(r->tpos + RB_D, q, sw, mode, at)) >> 15 : r->g1;
        }
        {                                              /* the next quarter point's value: interpolated, clipped, */
            int32_t s = r->y0 + (((r->y1 - r->y0) * (int32_t)k) / (int32_t)RB_D), g;   /* smoothed, shaped, ducked */
            r->pre1 += ((s - r->pre1) * fc) >> 15;     /* (the line's images at 1837 Hz +- the rumble, inharmonic: down
                                                        * before the clip, as its harmonics after it) */
            s = clamp(r->pre1 * 4, -32000, 32000);      /* (x16 of it < 2^31) */
            if (mk)
                s = (softclip((s * gd) >> 12) * mk) >> 15;
            r->post = clamp(r->post + (((s - r->post) * fc) >> 15), -40000, 40000);
            r->post2 = clamp(r->post2 + (((r->post - r->post2) * fc) >> 15), -40000, 40000);
            g = r->gc0 + (((r->gc1 - r->gc0) * (int32_t)k) / (int32_t)RB_D);
            g = (g * (lv >> 1)) >> 14;                 /* (the duck, the shape and RUMBLE, < 48400) */
            if (r->fade)                               /* (a clear on its way: down in 3 ms, 0 at its last point) */
                g = (g * (r->fade - 1)) >> 5;
            s = (r->post2 * g) >> 14;                  /* (|post2| <= 40000, g < 48400: < 2^31) */
            inc = (s - cur) / (int32_t)RB_Q;
            if (r->fade && !--r->fade) {               /* faded out (its last point at 0): cleared, from silence */
                r->cur = cur;
                r->inc = inc;
                r->k = (uint8_t)k;
                rb_clear(r, pre, 1);
                acc = 0;
            }
        }
    }
    r->acc = acc;
    r->cur = cur;
    r->inc = inc;
    r->k = (uint8_t)k;
}

/* (Gir 1.0) the kick's attack, DRUM's and DRUM-X's (EDIT > KICK): most of their kicks are round, an 808's body without a
 * click (tools/kick_measure.c). K.CLICK adds a tok (a sine falling from 2.4 kHz to the body in ~3 ms: the fast pitch
 * dive of a 909 or a hard techno kick) and a breath of noise (1.5 ms: the beater); K.PUNCH, a transient shaper: the
 * body's first ~10 ms up (x1 .. x4) and its rest down (to -6 dB, back over ~0.2 s: room for the rumble). Both into the
 * lane before its DRIVE, so a driven kit grinds them with the body. 0 and 0: nothing runs (bit for bit as before) */
typedef struct { uint32_t t, ph, inc; int32_t ce, ne, pe, se, ns; } ks_t;
static ks_t ks_st[NPART];
static void kick_hit(const track_t *t)
{
    ks_t *k = &ks_st[(uint32_t)(t - trk) % NPART];
    if (t->p[P_KCLICK] <= 0 && t->p[P_KPUNCH] <= 0)
        return;
    k->t = 0;
    k->ph = 0;
    k->inc = 233740000u;                               /* (2.4 kHz) */
    k->ce = k->ne = k->pe = k->se = 32767;
}
static __attribute__((noinline)) void kick_shape(const track_t *t, int32_t *y, uint32_t n)
{
    ks_t *k = &ks_st[(uint32_t)(t - trk) % NPART];
    int32_t c = t->p[P_KCLICK], pu = t->p[P_KPUNCH] * 4096 / 127, g = 4096;   /* (CLICK 0..127; PUNCH Q12 0..1) */
    uint32_t i;
    if (k->t > 13230u || (pu <= 0 && k->t > 882u))     /* (the click 20 ms (-60 dB), the punch 0.3 s) */
        return;
    if (!k->ns)
        k->ns = 0x2545F491;
    c = k->t < 882u ? c * 200 : 0;                     /* (the tok's peak at 127: ~25000; the breath's half) */
    for (i = 0; i < n; i++) {
        int32_t s = y[i];
        if (pu > 0) {
            if (!(i & 3u)) {                           /* (the punch's gain every 4 samples: Q12, < 2^14) */
                g = 4096 + ((3 * pu * k->pe) >> 15) - ((((pu >> 1) * (32767 - k->pe)) >> 15) * k->se >> 15);
                k->pe -= k->pe >> 7;                   /* (the attack: ~12 ms) */
                k->se -= k->se >> 11;                  /* (the body's dip back up: ~0.19 s) */
            }
            s = (s * g) >> 12;                         /* (|y| < 2^16: < 2^30) */
        }
        if (c > 0) {
            int32_t nz = (int32_t)(noise32(&k->ns) >> 17) - 16384;
            s += (((sine_i(k->ph) * k->ce) >> 15) * c) >> 15;
            s += (((nz * k->ne) >> 15) * c) >> 16;
            k->ph += k->inc;
            if (k->inc > 9800000u)                     /* (down to ~100 Hz, ~3 ms a fall of e) */
                k->inc -= k->inc >> 7;
            k->ce -= k->ce >> 7;                       /* (the tok: ~3 ms) */
            k->ne -= k->ne >> 6;                       /* (the breath: ~1.5 ms) */
        }
        y[i] = s;
    }
    k->t += n;
}
