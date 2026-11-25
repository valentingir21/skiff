/* SPDX-License-Identifier: GPL-3.0-only */
/* (techno-engines) MINI: a Minimoog voice, FMini-1's. The engine is FMini-1's FMINI (Guilherme Ruppert,
 * github.com/ruppert/fmini-1, GPL-3.0-only: firmware/src/eng_fmini.c, as of its commit c39e186), first ported to Skiff by
 * skiff-atlatl (LIkst4, GPL-3.0-only), brought up to FMini-1 today here: three oscillators (TRI, SHARK, SAW, SQR, wide
 * and narrow PULSE, SIN; ranges LO 32' 16' 8' 4' 2'; a slow drift each) and noise (white or pink) -> the mixer, linear to
 * 12 dB over full scale and then clipped, as the CA-72 measured -> the 24 dB ladder modelled on the Minimoog's
 * (zero-delay feedback through the emphasis network's lag pole, a differential pair with its bias in front of each
 * stage: EMPHASIS past 8 sings a sine that follows the keys; its laws in eng_mini_filter.h) with its own contour
 * (ATTACK DECAY SUSTAIN, the release the DECAY time; the AMOUNT through the CA-72's taper, up to 6.3 octaves) and
 * keyboard tracking; the low-pass or the high-pass output; the LFO as the modulation, the mod wheel a vibrato.
 * The eight EDIT values are FMini-1's: CUTOFF EMPH CONTOUR F.ATK F.DEC F.SUS MOD SLOT (the patch: FMini-1's 0..127).
 * What MINI adds: the patch's oscillators are the track's values (core.h P_MN_*, the pages OSC 1, OSC 2, OSC 3,
 * MINI MOD: FMini-1's panel), filled from SLOT (mini_poll): turning SLOT loads that patch into them, editing one makes
 * the sound the track's own. HOME's KNOB 4 is GLIDE (CUTOFF EMPHASIS CONTOUR GLIDE, the Minimoog's own). A preset may
 * move the oscillators up (mini_preset_fill: FMini-1's basses sound an octave or two under the note; Skiff's sound at
 * it). State per voice as FMini-1's: s[0..3] the ladder, s[4] the lag pole (Q8), s[5..6] the drift, s[7] the contour;
 * the part's noise once a block. */
#define ENGI_MINI 17u            /* engines.c ENGINES[] (append-only) */
#define FMINI_POLY 4
#define FG_SIZE 32u
#define FMINI_NFACTORY 128u

enum {                           /* the patch's bytes (FMini-1's) */
    FG_R1, FG_R2, FG_R3, FG_W1, FG_W2, FG_W3, FG_F2, FG_F3, FG_L1, FG_L2, FG_L3, FG_LN, FG_SW1, FG_SW2, FG_TUNE, FG_MMIX,
    FG_KBD, FG_GLID, FG_DC2L, FG_DC2H, FG_DC3L, FG_DC3H, FG_BPML, FG_BPMH, FG_NUM
};
enum { FS_O1 = 1, FS_O2 = 2, FS_O3 = 4, FS_NZ = 8, FS_PINK = 16, FS_MOSC = 32, FS_MFLT = 64 };   /* FG_SW1 */
enum { FMS_GLIDE = 1, FMS_SQR = 2, FMS_HP = 4 };                                                 /* FG_SW2 */

static int32_t fmini_det(const uint8_t *g, uint32_t lo)
{
    int32_t m = g[lo] | (int32_t)(g[lo + 1] & 7) << 7;
    return g[lo + 1] & 8 ? -m : m;
}

#define FGP(r1, r2, r3, w1, w2, w3, f2, f3, l1, l2, l3, ln, s1, s2, tn, mm, kb, gl, d2l, d2h, d3l, d3h, bl, bh) \
    {r1, r2, r3, w1, w2, w3, f2, f3, l1, l2, l3, ln, s1, s2, tn, mm, kb, gl, d2l, d2h, d3l, d3h, bl, bh}
#define ALLO (FS_O1 | FS_O2 | FS_O3)

#include "mini/eng_mini_patches.h"
#include "mini/eng_mini_filter.h"
#undef FGP

static int32_t fmini_nst[NTRK] = {0x2F6E2B1, 0x1A2B3C5, 0x3C4D5E7, 0x0F1E2D3, 0x2468ACE, 0x13579BD};
static int32_t fmini_pk[NTRK][3];
static int32_t fmini_nbuf[NTRK][CTL];
static int32_t fmini_sm[NTRK][3];
static uint8_t fmini_sm_ok;
static int32_t fmini_mn[NTRK];


/* SLOT 0..127: FMini-1's patch of that number */
static const uint8_t *fmini_g(const track_t *t) { return FMINI_FACTORY[(uint32_t)clamp(t->p[P_E7], 0, FMINI_NFACTORY - 1)]; }

/* what a block plays: the patch's oscillators, or the track's own (P_MN_SET) */
typedef struct {
    uint8_t r[3], w[3], kbd, mosc, mflt, nz, pink, hp, sqr, mmix, tune;
    int16_t det[2];                                   /* cents */
    int32_t l[3], ln;                                 /* the mixer's levels, Q15 (a squared curve) */
} mini_cfg_t;
static void mini_cfg(const track_t *t, mini_cfg_t *c)
{
    const int16_t *p = t->p;
    const uint8_t *g = fmini_g(t);
    uint32_t sw = g[FG_SW1], k;
    c->sqr = (g[FG_SW2] & FMS_SQR) != 0;
    c->mmix = g[FG_MMIX];
    c->tune = g[FG_TUNE];
    if (!p[P_MN_SET]) {                               /* the patch */
        for (k = 0; k < 3u; k++) {
            c->r[k] = (uint8_t)(g[FG_R1 + k] % 6u);
            c->w[k] = (uint8_t)(g[FG_W1 + k] % 7u);
            c->l[k] = (sw >> k) & 1u ? (int32_t)(g[FG_L1 + k] * g[FG_L1 + k] * 2) : 0;
        }
        c->det[0] = (int16_t)fmini_det(g, FG_DC2L);
        c->det[1] = (int16_t)fmini_det(g, FG_DC3L);
        c->ln = sw & FS_NZ ? (int32_t)(g[FG_LN] * g[FG_LN] * 2) : 0;
        c->pink = (sw & FS_PINK) != 0;
        c->mosc = (sw & FS_MOSC) != 0;
        c->mflt = (sw & FS_MFLT) != 0;
        c->kbd = g[FG_KBD];
        c->hp = (g[FG_SW2] & FMS_HP) != 0;
    } else {                                          /* the track's own (OSC 1 .. MINI MOD) */
        static const uint8_t R[3] = {P_MN_R1, P_MN_R2, P_MN_R3}, W[3] = {P_MN_W1, P_MN_W2, P_MN_W3};
        static const uint8_t L[3] = {P_MN_L1, P_MN_L2, P_MN_L3};
        for (k = 0; k < 3u; k++) {
            int32_t lv = clamp(p[L[k]], 0, 127);
            c->r[k] = (uint8_t)clamp(p[R[k]], 0, 5);
            c->w[k] = (uint8_t)clamp(p[W[k]], 0, 6);
            c->l[k] = lv * lv * 2;
        }
        c->det[0] = (int16_t)(clamp(p[P_MN_D2], -64, 63) * 11);
        c->det[1] = (int16_t)(clamp(p[P_MN_D3], -64, 63) * 11);
        c->ln = clamp(p[P_MN_NZ], 0, 127) * clamp(p[P_MN_NZ], 0, 127) * 2;
        c->pink = p[P_MN_NCOL] != 0;
        c->mosc = (p[P_MN_MDST] & 1) != 0;
        c->mflt = (p[P_MN_MDST] & 2) != 0;
        c->kbd = (uint8_t)clamp(p[P_MN_KBD], 0, 127);
        c->hp = p[P_MN_FMODE] != 0;
    }
}

/* ------------------------------------------------- the track's oscillators (main loop) --- */
/* patch g's oscillators as the track's values (OSC 1 .. MINI MOD) */
static void mini_fill(int16_t *p, const uint8_t *g)
{
    static const uint8_t R[3] = {P_MN_R1, P_MN_R2, P_MN_R3}, W[3] = {P_MN_W1, P_MN_W2, P_MN_W3};
    static const uint8_t L[3] = {P_MN_L1, P_MN_L2, P_MN_L3};
    uint32_t sw = g[FG_SW1], k;
    int32_t d;
    for (k = 0; k < 3u; k++) {
        p[R[k]] = (int16_t)(g[FG_R1 + k] % 6u);
        p[W[k]] = (int16_t)(g[FG_W1 + k] % 7u);
        p[L[k]] = (int16_t)((sw >> k) & 1u ? g[FG_L1 + k] : 0u);
    }
    d = fmini_det(g, FG_DC2L);
    p[P_MN_D2] = (int16_t)clamp((d + (d < 0 ? -5 : 5)) / 11, -64, 63);   /* (11 cents a step: FMini-1's coarse byte) */
    d = fmini_det(g, FG_DC3L);
    p[P_MN_D3] = (int16_t)clamp((d + (d < 0 ? -5 : 5)) / 11, -64, 63);
    p[P_MN_NZ] = (int16_t)(sw & FS_NZ ? g[FG_LN] : 0u);
    p[P_MN_NCOL] = (sw & FS_PINK) != 0;
    p[P_MN_MDST] = (int16_t)(((sw & FS_MOSC) ? 1 : 0) | ((sw & FS_MFLT) ? 2 : 0));
    p[P_MN_KBD] = g[FG_KBD];
    p[P_MN_FMODE] = (g[FG_SW2] & FMS_HP) != 0;
}
/* the values a patch gives (mini_fill) are what the track holds: OSC 1 .. MINI MOD unedited */
static int mini_same(const int16_t *p, const uint8_t *g)
{
    int16_t q[P_COUNT];
    uint32_t i;
    mini_fill(q, g);
    for (i = P_MN_R1; i <= P_MN_FMODE; i++)
        if (q[i] != p[i])
            return 0;
    return 1;
}
#define MINI_UNSEEN 0xFFFFu
static uint16_t mini_slot[NTRK] = {MINI_UNSEEN, MINI_UNSEEN, MINI_UNSEEN, MINI_UNSEEN, MINI_UNSEEN, MINI_UNSEEN};
/* a load put track tr's values in (a preset, a project, a user sound, an undo): take its SLOT and oscillators as they
 * are, no patch loads over them (ui.c load_end, project.c, the undo) */
static void mini_forget(uint32_t tr) { mini_slot[tr % NTRK] = MINI_UNSEEN; }
/* main loop, every frame: SLOT turned (a knob, the editor, MIDI, motion) -> that patch into OSC 1 .. MINI MOD (P_MN_SET
 * 0: the patch plays as it is); an oscillator edited -> P_MN_SET 1, the track's own */
static void mini_poll(void)
{
    uint32_t tr;
    for (tr = 0; tr < NTRK; tr++) {
        track_t *t = &trk[tr];
        int32_t s = clamp(t->p[P_E7], 0, FMINI_NFACTORY - 1);
        if (t->eng_req != ENGI_MINI) {
            mini_slot[tr] = MINI_UNSEEN;
            continue;
        }
        if (mini_slot[tr] == MINI_UNSEEN) {           /* as loaded: a patch as it is, or the track's own */
            if (!t->p[P_MN_SET])
                mini_fill(t->p, FMINI_FACTORY[s]);
            mini_slot[tr] = (uint16_t)s;
        } else if (s != mini_slot[tr]) {              /* SLOT turned: that patch */
            t->p[P_MN_SET] = 0;
            mini_fill(t->p, FMINI_FACTORY[s]);
            mini_slot[tr] = (uint16_t)s;
        } else if (!t->p[P_MN_SET] && !mini_same(t->p, FMINI_FACTORY[s]))
            t->p[P_MN_SET] = 1;                       /* an oscillator edited: the track's own from now on */
    }
}

/* ------------------------------------------------------------- the presets --- */
/* each MINI preset's patch (FMini-1's SLOT), and whether it is a bass: FMini-1's basses sound an octave or two under
 * the note (its keyboard sits there); Skiff's sound at it, so a bass preset moves its oscillators up until the lowest
 * one that sounds is at 8' (the relations between them stay) */
static const struct { uint8_t slot, bass; } MINI_PSLOT[] = {
    {1, 1}, {38, 1}, {32, 1}, {35, 1}, {36, 1}, {34, 1}, {0, 0}, {110, 0}, {112, 0}, {15, 0},
};
/* ui.c preset_fill: preset pi's SLOT and its oscillators (a bass's moved up: the track's own) */
static void mini_preset_fill(int16_t *p, uint32_t pi)
{
    static const uint8_t R[3] = {P_MN_R1, P_MN_R2, P_MN_R3}, L[3] = {P_MN_L1, P_MN_L2, P_MN_L3};
    uint32_t k, s = MINI_PSLOT[pi % NELEM(MINI_PSLOT)].slot;
    int32_t lo = 5, up;
    p[P_E7] = (int16_t)s;
    mini_fill(p, FMINI_FACTORY[s]);
    for (k = 0; k < 3u; k++)
        if (p[L[k]] && p[R[k]] < lo)
            lo = p[R[k]];
    up = MINI_PSLOT[pi % NELEM(MINI_PSLOT)].bass && lo < 3 ? 3 - lo : 0;
    p[P_MN_SET] = (int16_t)(up != 0);
    for (k = 0; k < 3u; k++)
        p[R[k]] = (int16_t)clamp(p[R[k]] + up, 0, 5);
}

/* ------------------------------------------------------------------ the block --- */
static void fmini_block(track_t *t)
{
    uint32_t tr = (uint32_t)(t - trk), i;
    mini_cfg_t c;
    if (tr >= NTRK)
        return;
    for (i = 0; i < NVOICE && !t->v[i].active; i++)
        ;
    {   /* the knobs' steps glide (FMini-1: 200 ms); silent: snap */
        static const uint8_t KN[3] = {P_E0, P_E1, P_E2};
        uint32_t j;
        for (j = 0; j < 3; j++) {
            int32_t want = (int32_t)clamp(t->p[KN[j]], 0, 127) << 16, d = want - fmini_sm[tr][j], st;
            if (i == NVOICE || !(fmini_sm_ok & (1u << tr))) {
                fmini_sm[tr][j] = want;
                continue;
            }
            st = d >= 0 ? (int32_t)(((int64_t)d * 238) >> 16) : -(int32_t)(((int64_t)-d * 238) >> 16);
            if (!st)
                st = d > 0 ? 1 : d < 0 ? -1 : 0;
            fmini_sm[tr][j] += st;
        }
        if (i == NVOICE)
            fmini_sm_ok &= (uint8_t)~(1u << tr);
        else
            fmini_sm_ok |= (uint8_t)(1u << tr);
    }
    if (i == NVOICE)
        return;
    mini_cfg(t, &c);
    fmini_mn[tr] = (int32_t)(noise32(&fmini_nst[tr]) >> 17) - 16384;
    if (c.ln) {
        int32_t b0 = fmini_pk[tr][0], b1 = fmini_pk[tr][1], b2 = fmini_pk[tr][2];
        for (i = 0; i < CTL; i++) {
            int32_t w = (int32_t)(noise32(&fmini_nst[tr]) >> 16) - 32768;
            if (c.pink) {
                b0 += ((w * 3245) >> 15) - ((b0 * 77) >> 15);
                b1 += ((w * 9716) >> 15) - ((b1 * 1212) >> 15);
                b2 += ((w * 34495) >> 15) - ((b2 * 14090) >> 15);
                w = (b0 + b1 + b2 + ((w * 6056) >> 15)) >> 1;
            }
            fmini_nbuf[tr][i] = w;
        }
        fmini_pk[tr][0] = b0;
        fmini_pk[tr][1] = b1;
        fmini_pk[tr][2] = b2;
    }
}

/* ------------------------------------------------------------------ the voice --- */
static inline int32_t fmini_shark(uint32_t ph)
{
    uint32_t x = ph >> 16;
    return x < 49152u ? (int32_t)((x * 43691u) >> 15) - 32768 : 32767 - (int32_t)((x - 49152u) * 4u);
}

static void fmini_wave(uint32_t w, uint32_t *php, uint32_t inc, int32_t g, int32_t *mix, uint32_t n)
{
    uint32_t ph = *php, i;
    switch (w) {
    case 0:
        for (i = 0; i < n; i++, ph += inc)
            mix[i] += mulq15(osc_tri(ph), g);
        break;
    case 1:
        for (i = 0; i < n; i++, ph += inc)
            mix[i] += mulq15(fmini_shark(ph), g);
        break;
    case 2:
        for (i = 0; i < n; i++, ph += inc)
            mix[i] += mulq15(osc_saw(ph, inc), g);
        break;
    case 3:
        for (i = 0; i < n; i++, ph += inc)
            mix[i] += mulq15(osc_pulse(ph, inc, 0x80000000u), g);
        break;
    case 4:
        for (i = 0; i < n; i++, ph += inc)
            mix[i] += mulq15(osc_pulse(ph, inc, 0x55555555u), g);
        break;
    case 6:                                           /* (FMini-1 today) SIN: the sine table, no harmonics to fold */
        for (i = 0; i < n; i++, ph += inc)
            mix[i] += mulq15(osc_sine(ph), g);
        break;
    default:
        for (i = 0; i < n; i++, ph += inc)
            mix[i] += mulq15(osc_pulse(ph, inc, 0x1C71C71Cu), g);
        break;
    }
    *php = ph;
}

static int32_t fmini_range16(uint32_t r)
{
    static const int16_t R[6] = {-48 * 16, -24 * 16, -12 * 16, 0, 12 * 16, 24 * 16};
    return R[r % 6u];
}

static void fmini_note_on(track_t *t, voice_t *v)
{
    uint32_t tr = (uint32_t)(t - trk) % NTRK;
    if (!v->env && !v->env_out) {
        v->s[0] = v->s[1] = v->s[2] = v->s[3] = 0;
        v->s[4] = 0;
        v->s[7] = 0;
        if (t->p[P_E1] >= 96)
            v->s[0] = (noise32(&fmini_nst[tr]) & 0x80000000u) ? 700 * 256 : -700 * 256;
        v->s[5] = (int32_t)(noise32(&fmini_nst[tr]) >> 20) - 2048;
        v->s[6] = (int32_t)(noise32(&fmini_nst[tr]) >> 20) - 2048;
    }
    v->s[7] = (int32_t)(1u << 24) | (v->s[7] & 0xFFFFFF);
}

/* the contour, once a block: stage 1 attack (a capacitor charging toward a level above full: concave, a corner at the
 * top, as the Minimoog's), 2 decay to the sustain, 3 release (the DECAY time); level Q23 */
static int32_t fmini_contour(const int16_t *p, voice_t *v)
{
    uint32_t st = (uint32_t)v->s[7] >> 24;
    int32_t e = v->s[7] & 0xFFFFFF, sus = (int32_t)p[P_E5] << 16, k = (int32_t)ENV_EXP[p[P_E4] & 127];
    if (!v->gate && st && st != 3u)
        st = 3;
    switch (st) {
    case 1: {
        int32_t step = (int32_t)(ENV_LIN[p[P_E3] & 127] >> 1), d = (int32_t)(((int64_t)((5 << 21) - e) * step * 824) >> 32);
        e += d < 1 ? 1 : d;
        if (e >= (1 << 23)) {
            e = 1 << 23;
            st = 2;
        }
        break;
    }
    case 2:
        e += mulq16(sus - e, (uint32_t)k);
        break;
    case 3:
        e -= mulq16(e, (uint32_t)k);
        break;
    default:
        break;
    }
    v->s[7] = (int32_t)(st << 24) | (e & 0xFFFFFF);
    return e >> 8;
}

static inline int32_t fmini_mulr(int32_t a, uint32_t k)
{
    return (int32_t)(((a >> 16) * (int32_t)k) + (int32_t)(((uint32_t)(a & 0xFFFF) * k + 32768u) >> 16));
}

/* (FMini-1 today) the ladder's input is the mixer's sum 12 dB down (the CA-72's ladder runs under its pairs' knee: a
 * sawtooth comes out as drawn), its output raised by the same; each pair has its bias (its 2nd harmonic at full level
 * -37 dB, the CA-72's), taken back out */
#define FMINI_LADDER_IN 2
#define FMINI_PAIR_BIAS 1500
static inline int32_t pairb(int32_t x, int32_t pair_off) { return softclip(x + FMINI_PAIR_BIAS) - pair_off; }

static void fmini_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    mini_cfg_t c;
    int32_t mix[CTL];
    int32_t env15, mod, lfo, vib, lc, p16, fine, k12, G16, gx16, c0, c1, c2, c3, inv14, kinv12, trim12, pair_off;
    int32_t dr1, dr2, dr3;
    uint32_t tr = (uint32_t)(t - trk) % NTRK, i;
    uint32_t inc1, inc2, inc3;
    int32_t s0 = v->s[0], s1 = v->s[1], s2 = v->s[2], s3 = v->s[3], sx = v->s[4];
    uint32_t ph0 = v->ph[0], ph1 = v->ph[1], ph2 = v->ph[2];
    if (n > CTL)
        n = CTL;
    mini_cfg(t, &c);
    env15 = fmini_contour(p, v);
    lfo = mulq15(t->lfo_val, t->lfo_fade);              /* (the block's LFO, as the voices' routings hear it: voice.c) */
    if (c.sqr)
        lfo = lfo >= 0 ? 32767 : -32767;
    mod = mulq15(lfo, 32767 - (int32_t)c.mmix * 258) + mulq15(fmini_mn[tr] << 1, (int32_t)c.mmix * 258);
    mod = mulq15(mod, (int32_t)p[P_E6] * 258);
    vib = (mulq15(lfo, (int32_t)t->mw * 258) * 16) >> 15;
    p16 = m->pitch16 + vib + (c.mosc ? (mod * 16) >> 14 : 0);
    fine = m->fine + ((int32_t)c.tune - 64) * 237 / 64;
    {
        int32_t d1 = v->s[5], d2 = v->s[6];
        d1 += ((int32_t)(noise32(&fmini_nst[tr]) >> 20) - 2048 >> 6) - (d1 >> 11);
        d2 += ((int32_t)(noise32(&fmini_nst[tr]) >> 20) - 2048 >> 6) - (d2 >> 11);
        v->s[5] = d1;
        v->s[6] = d2;
        dr1 = d1 >> 8;
        dr2 = d2 >> 8;
        dr3 = (d1 - d2) >> 9;
    }
    inc1 = cents_inc(clamp(p16 + fmini_range16(c.r[0]), 0, 2047), dr1, fine);
    inc2 = cents_inc(clamp(p16 + fmini_range16(c.r[1]), 0, 2047), c.det[0] + dr2, fine);
    inc3 = cents_inc(clamp(p16 + fmini_range16(c.r[2]), 0, 2047), c.det[1] + dr3, fine);
    for (i = 0; i < n; i++)
        mix[i] = 0;
    if (c.l[0]) fmini_wave(c.w[0], &ph0, inc1, c.l[0], mix, n); else ph0 += inc1 * n;
    if (c.l[1]) fmini_wave(c.w[1], &ph1, inc2, c.l[1], mix, n); else ph1 += inc2 * n;
    if (c.l[2]) fmini_wave(c.w[2], &ph2, inc3, c.l[2], mix, n); else ph2 += inc3 * n;
    if (c.ln)
        for (i = 0; i < n; i++)
            mix[i] += mulq15(fmini_nbuf[tr][i], c.ln);
    {
        int32_t kn = fmini_sm[tr][0], i0 = kn >> 16, fr = (kn >> 8) & 255;
        lc = FMINI_CUT[i0];
        if (i0 < 127)
            lc += ((FMINI_CUT[i0 + 1] - lc) * fr) >> 8;
    }
    lc += (m->cutoff * 55) >> 10;
    {   /* (FMini-1 today) the contour: the CA-72's AMOUNT taper, up to 6.3 octaves */
        uint32_t ci = (uint32_t)fmini_sm[tr][2] >> 16, cf = ((uint32_t)fmini_sm[tr][2] & 0xFFFFu) >> 1;
        int32_t ctap;
        if (ci > 126u) {
            ci = 126u;
            cf = 32768u;
        }
        ctap = (int32_t)FMINI_CNTR[ci] + (int32_t)((((int32_t)FMINI_CNTR[ci + 1u] - (int32_t)FMINI_CNTR[ci]) * (int32_t)cf) >> 15);
        lc += (int32_t)(((int64_t)env15 * ctap * FMINI_CNTR_FULL16) >> 30);
    }
    lc += ((v->pitch_cur - FMINI_NOTE0 * 16) * (int32_t)c.kbd * 516) >> 16;
    if (c.mflt)
        lc += (mod * 547) >> 15;
    lc = clamp(lc, FMINI_GM0 * 16, FMINI_FOSC_MAX16);
    {
        uint32_t ix = (uint32_t)((lc >> 4) - FMINI_GM0), fr = (uint32_t)lc & 15u, gq, gxq, g2, g3, g4, om;
        gq = FMINI_G[ix] + (uint32_t)(((int32_t)(FMINI_G[ix + 1] - FMINI_G[ix]) * (int32_t)fr) >> 4);
        gxq = FMINI_GX[ix] + (uint32_t)(((int32_t)(FMINI_GX[ix + 1] - FMINI_GX[ix]) * (int32_t)fr) >> 4);
        G16 = (int32_t)((gq + 128u) >> 8);
        gx16 = (int32_t)((gxq + 128u) >> 8);
        om = 65536u - (uint32_t)G16;
        g2 = ((uint32_t)G16 * (uint32_t)G16) >> 16;
        g3 = (g2 * (uint32_t)G16) >> 16;
        g4 = (g3 * (uint32_t)G16) >> 16;
        c3 = (int32_t)(om >> 2);
        c2 = (int32_t)(((uint32_t)G16 * om) >> 18);
        c1 = (int32_t)((g2 * om) >> 18);
        c0 = (int32_t)((g3 * om) >> 18);
        {
            int32_t kn = fmini_sm[tr][1], i0 = kn >> 16, fk = (kn >> 8) & 255, den;
            k12 = FMINI_K[i0];
            trim12 = FMINI_TRIM[i0];
            if (i0 < 127) {
                k12 += ((FMINI_K[i0 + 1] - k12) * fk) >> 8;
                trim12 += ((FMINI_TRIM[i0 + 1] - trim12) * fk) >> 8;
            }
            den = 65536 + ((k12 * (int32_t)((((uint32_t)gxq >> 8) * g4) >> 16)) >> 12);
            inv14 = (int32_t)((1u << 30) / (uint32_t)den);
            kinv12 = (k12 * inv14) >> 14;
        }
    }
    pair_off = softclip(FMINI_PAIR_BIAS);
    for (i = 0; i < n; i++) {
        int32_t x = clamp(mix[i], -(32767 << FMINI_LADDER_IN), 32767 << FMINI_LADDER_IN) >> FMINI_LADDER_IN;
        int32_t S, fbc, u, xin, v1, v2, v3, v4, y1, y2, y3, y4, w, o;
        S = (c0 * ((s0 + 128) >> 8) + c1 * ((s1 + 128) >> 8) + c2 * ((s2 + 128) >> 8) + c3 * ((s3 + 128) >> 8)) >> 14;
        fbc = sx + fmini_mulr((S << 8) - sx, (uint32_t)gx16);
        u = ((x * inv14) >> 14) - ((kinv12 * ((fbc + 128) >> 8)) >> 12);
        xin = pairb(u, pair_off) << 8;
        v1 = fmini_mulr(xin - s0, (uint32_t)G16);
        y1 = v1 + s0;
        s0 = y1 + v1;
        v2 = fmini_mulr((pairb((y1 + 128) >> 8, pair_off) << 8) - s1, (uint32_t)G16);
        y2 = v2 + s1;
        s1 = y2 + v2;
        v3 = fmini_mulr((pairb((y2 + 128) >> 8, pair_off) << 8) - s2, (uint32_t)G16);
        y3 = v3 + s2;
        s2 = y3 + v3;
        v4 = fmini_mulr((pairb((y3 + 128) >> 8, pair_off) << 8) - s3, (uint32_t)G16);
        y4 = v4 + s3;
        s3 = y4 + v4;
        w = fmini_mulr(y4 - sx, (uint32_t)gx16);
        sx += w + w;
        o = c.hp ? xin - 4 * y1 + 6 * y2 - 4 * y3 + y4 : y4;   /* HP: (1 - H)^4 on the stages, as FMini-1 */
        out[i] += voice_amp2((((o + 128) >> 8) * trim12 * 5) >> 12, m, i);   /* (x 4: the ladder's 12 dB back; x 1.25: level
                                                                               * with ANALOG's, as the MINI before) */
    }
    v->ph[0] = ph0;
    v->ph[1] = ph1;
    v->ph[2] = ph2;
    v->s[0] = s0;
    v->s[1] = s1;
    v->s[2] = s2;
    v->s[3] = s3;
    v->s[4] = sx;
}

static const preset_t MINI_PRESETS[] = {
    /* name, {CUT, EMPH, CNTR, F.ATK, F.DEC, F.SUS, MOD, SLOT (mini_preset_fill: MINI_PSLOT)}, {A D S R}, fenv, mono,
     * sends, more, type: FMini-1's patches and their knobs (MOOG BASS, FUZZ BASS, SUB BASS, SEQ BASS, REESE, OCTAVE PUMP,
     * MINI LEAD, OVERDRIVE, SAW WALL, ACID LEAD), techno's envelopes and sends */
    {"BS MOOG", {43, 51, 57, 0, 70, 30, 0, 0}, {0, 74, 100, 30}, 0, 1, FX(20, 0, 0, 10), MORE({P_GLIDE, 18}), TAG(PT_BASS)},
    {"BS MOOG HARD", {56, 34, 24, 0, 72, 60, 0, 0}, {0, 75, 115, 30}, 0, 1, FX(100, 0, 0, 8), MORE({P_DTYPE, 1}), TAG(PT_BASS)},
    {"BS MOOG SUB", {38, 8, 12, 0, 72, 60, 0, 0}, {0, 77, 118, 30}, 0, 1, FX(10, 0, 0, 0), TAG(PT_BASS)},
    {"BS MOOG PLCK", {47, 61, 57, 0, 64, 0, 0, 0}, {0, 67, 0, 50}, 0, 1, FX(30, 0, 40, 15), TAG(PT_BASS)},
    {"BS MG REESE", {43, 32, 19, 0, 84, 60, 30, 0}, {20, 81, 120, 50}, 0, 1, FX(50, 60, 0, 20), TAG(PT_BASS)},
    {"BS MOOG OCT", {53, 32, 60, 0, 61, 0, 0, 0}, {0, 69, 40, 30}, 0, 1, FX(30, 0, 25, 12), TAG(PT_BASS)},
    {"LD MOOG", {61, 44, 44, 0, 79, 100, 30, 0}, {5, 81, 110, 60}, 0, 1, FX(20, 0, 55, 40), MORE({P_GLIDE, 20}), TAG(PT_LEAD)},
    {"LD MOOG DIRT", {57, 66, 40, 0, 84, 60, 22, 0}, {0, 75, 120, 60}, 0, 1, FX(75, 0, 45, 30), MORE({P_GLIDE, 16}), TAG(PT_LEAD)},
    {"LD MOOG TRNC", {54, 56, 47, 0, 81, 50, 30, 0}, {10, 84, 115, 70}, 0, 1, FX(30, 40, 50, 50), MORE({P_GLIDE, 18}), TAG(PT_LEAD)},
    {"LD MOOG ACID", {32, 102, 42, 0, 69, 5, 0, 0}, {0, 72, 110, 57}, 0, 1, FX(70, 0, 55, 0), TAG(PT_LEAD)},
};
_Static_assert(NELEM(MINI_PRESETS) == NELEM(MINI_PSLOT), "MINI: a patch for each preset");

static const engine_t ENG_MINI = {
    .name = "MINI",
    .page_title = {"FILTER", "PATCH"},
    .edit = {
        {"CUTOFF", 0, 0, 127, 64, F_PCT, 0},
        {"EMPH", 0, 0, 127, 40, F_PCT, 0},
        {"CONTOUR", 0, 0, 127, 64, F_PCT, 0},
        {"F.ATK", 0, 0, 127, 0, F_PCT, 0},
        {"F.DEC", 0, 0, 127, 64, F_PCT, 0},
        {"F.SUS", 0, 0, 127, 40, F_PCT, 0},
        {"MOD", 0, 0, 127, 0, F_PCT, 0},
        {"SLOT", 0, 0, FMINI_NFACTORY - 1, 1, F_INT, 0},   /* FMini-1's patch */
    },
    .presets = MINI_PRESETS,
    .npresets = NELEM(MINI_PRESETS),
    .note_on = fmini_note_on,
    .render = fmini_render,
    .knob = {P_E0, P_E1, P_E2, P_GLIDE},
    .poly = FMINI_POLY,
    .keep = 0x13,
    .block = fmini_block,
};
