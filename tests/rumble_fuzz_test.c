/* SPDX-License-Identifier: GPL-3.0-only */
/* (Gir 1.0) the RUMBLE's stability and its highs (rumble.c), each case in a process of its own (from power-on):
 *   - R.TONE swept 0 .. 127 .. 0, slowly then a step a block, playing, on every kit with a RUMBLE, with the other
 *     values at their ends: no frozen output (the rumble's level stays tied to the kicks: low just after each), no
 *     state held at its bounds, the tail ends and the kick's voice with it once the kicks stop;
 *   - N random cases of every RUMBLE value (RUMBLE R.TUNE R.DECAY R.DRIVE R.TONE R.DUCK R.ATTACK R.SLICE), the
 *     extremes half the time, values turned every few blocks (as automation, locks would), a kit load, STOP / PLAY:
 *     no output out of range, nothing that does not end, a kit load and a PLAY clear the state (rumble.c fade);
 *   - the metallic noise: every kit with a RUMBLE at RUMBLE, R.DRIVE and R.TONE 127, the track's DRIVE at 90 and
 *     its LEVEL at 127: the rumble alone (RUMBLE 127 less RUMBLE 1: the same voices) has its energy below 400 Hz,
 *     under -45 dB of it above 1 kHz (no fizz, no inharmonic lines), and the track's DRIVE does not touch it.
 *   cc -O2 -w -Ibuild/gen -Ifirmware/src -o build/host/rumble_fuzz_test tests/rumble_fuzz_test.c -lm
 *   build/host/rumble_fuzz_test [CASES] [SEED] */
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

static int bad;
static void check(const char *what, int ok)
{
    printf("rumble_fuzz: %-92s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}

#define NOUT (FS * 4u)
typedef struct { int32_t err; double tail, after, mid; uint32_t sat, end; char msg[160]; float y[NOUT]; } res_t;
static res_t *R;

static uint32_t kit_e(uint32_t k) { return RB_KITS[k][0] >= 128u ? ENGI_DRUMX : ENGI_DRUM; }
static uint32_t kit_pi(uint32_t k) { return RB_KITS[k][0] & 127u; }

static void steps4(track_t *t)
{
    uint32_t i;
    for (i = 0; i < 16u; i++) {
        uint8_t k = 36;
        put_step(t, i, i % 4u == 0u, &k, i % 4u == 0u ? ST_NOTE : ST_REST, 0);
    }
}
static void setup(track_t *t, uint32_t e, uint32_t pi)
{
    host_tracks_init();
    song.g[G_BPM] = 150;
    host_preset(t, e, pi);
    t->p[P_AMODE] = 0;
    t->p[P_SLEN] = 16;
    t->p[P_SDIV] = 2;
    t->p[P_CHOR] = t->p[P_DLY] = t->p[P_REV] = 0;
    steps4(t);
}
/* one block: the output's left, its bounds checked */
static void blk(int32_t *y)
{
    int32_t o[2 * CTL];
    uint32_t i;
    mix_block(o, CTL);
    for (i = 0; i < CTL; i++) {
        if (o[2 * i] > 32767 || o[2 * i] < -32768)
            R->err = 1;
        if (y)
            y[i] = o[2 * i];
    }
}
static int voices(const track_t *t)
{
    int n = 0;
    uint32_t i;
    for (i = 0; i < NVOICE; i++)
        n += t->v[i].active;
    return n;
}

/* the R.TONE sweep on kit k: speed 0 slow (2 s up, 2 s down), 1 a step a block; ends: the other values at their ends */
static void sweep(uint32_t k, int fast, int ends)
{
    track_t *t = &trk[0];
    int32_t y[CTL];
    uint32_t b, nb = FS * 4u / CTL, i, beat = FS * 60u / 150u, n = 0;
    double af = 0, mi = 0;
    rb_t *r;
    setup(t, kit_e(k), kit_pi(k));
    if (ends) {
        t->p[P_RB_MIX] = 127; t->p[P_RB_DECAY] = 127; t->p[P_RB_DRIVE] = 127; t->p[P_RB_DUCK] = 1;
        t->p[P_RB_TUNE] = 0; t->p[P_RB_ATK] = 127; t->p[P_DIST] = 127;
    }
    transport_req = 1;
    r = rb_of(t);
    for (b = 0; b < nb; b++) {
        int32_t tn = fast ? (int32_t)(b & 1u) * 127 : (int32_t)(b < nb / 2u ? b * 254u / nb : (nb - b) * 254u / nb);
        t->p[P_RB_TONE] = (int16_t)clamp(tn, 0, 127);
        if (b == nb / 2u)
            t->p[P_RB_TONE] = 127;
        blk(y);
        for (i = 0; i < CTL; i++) {
            uint32_t ph = (b * CTL + i) % beat;
            double v = (double)y[i] * y[i];
            if (b * CTL > FS && ph < FS / 200u)
                af += v;
            else if (b * CTL > FS && ph > beat / 3u && ph < beat * 2u / 3u)
                mi += v, n++;
            R->y[(b * CTL + i) % NOUT] = (float)y[i];
        }
        R->sat += r->bad > 100u;
    }
    R->after = af;
    R->mid = mi / (n | 1u);
    transport_req = 2;                                 /* STOP: the tail ends, the voice with it */
    for (b = 0; b < FS * 6u / CTL; b++) {
        blk(y);
        if (b > FS * 5u / CTL)
            for (i = 0; i < CTL; i++)
                R->tail += (double)y[i] * y[i];
    }
    R->end = (uint32_t)voices(t);
}

static uint32_t rnd(uint32_t *s) { *s ^= *s << 13; *s ^= *s >> 17; *s ^= *s << 5; return *s; }
static int16_t pick(uint32_t *s, int16_t lo, int16_t hi)
{
    uint32_t x = rnd(s) % 4u;
    return x == 0u ? lo : x == 1u ? hi : (int16_t)(lo + (int16_t)(rnd(s) % (uint32_t)(hi - lo + 1)));
}
/* random case c: every value, turned every few blocks; a kit load and STOP / PLAY on the way */
static void fuzz(uint32_t c, uint32_t seed)
{
    static const int16_t LO[8] = {0, -1, 0, 0, 0, 0, 0, 0}, HI[8] = {127, 11, 127, 127, 127, 127, 127, 4};
    track_t *t = &trk[0];
    uint32_t s = seed * 2654435761u + c * 40503u + 1u, k = rnd(&s) % NELEM(RB_KITS), b, j, cleared = 1;
    int32_t y[CTL];
    rb_t *r;
    setup(t, kit_e(k), kit_pi(k));
    song.g[G_BPM] = (int16_t)(60 + rnd(&s) % 181u);
    t->p[P_SSWING] = (int16_t)(rnd(&s) % 2u ? rnd(&s) % 100u : 0);
    t->p[P_DIST] = pick(&s, 0, 127);
    t->p[P_LEVEL] = pick(&s, 0, 127);
    transport_req = 1;
    r = rb_of(t);
    for (b = 0; b < FS * 3u / CTL; b++) {
        if (b % (1u + rnd(&s) % 40u) == 0u)
            for (j = 0; j < 8u; j++)
                if (rnd(&s) % 3u == 0u)
                    t->p[P_RB_MIX + j] = pick(&s, LO[j], HI[j]);
        if (b == FS / 2u / CTL) {                      /* a kit load: the state cleared at its next kick (a beat at most) */
            k = rnd(&s) % NELEM(RB_KITS);
            host_preset_req(t, kit_e(k), kit_pi(k));
        }
        if (b == FS * 17u / 10u / CTL) {
            cleared &= !r->on || (r->fade == 0 && r->pre == (uint16_t)(t->preset | (uint32_t)t->engine << 8));
            if (getenv("RBDBG"))
                fprintf(stderr, "load check: on %u fade %u pre %04x now %04x eng_req %u engine %u\n", r->on, r->fade, r->pre,
                       (unsigned)(t->preset | (uint32_t)t->engine << 8), t->eng_req, t->engine);
        }
        if (b == FS * 2u / CTL)
            transport_req = 2;
        if (b == FS * 2u / CTL + 20u)
            transport_req = 1;
        if (b == FS * 5u / 2u / CTL) {
            cleared &= !r->on || (r->fade == 0 && r->play == rb_play_gen);
            if (getenv("RBDBG"))
                fprintf(stderr, "play check: on %u fade %u play %u gen %u\n", r->on, r->fade, r->play, rb_play_gen);
        }
        blk(y);
        R->sat += r->bad > 1000u;
    }
    if (!cleared)
        R->err = 2;
    transport_req = 2;
    for (b = 0; b < FS * 8u / CTL; b++) {
        blk(y);
        if (b > FS * 7u / CTL)
            for (j = 0; j < CTL; j++)
                R->tail += (double)y[j] * y[j];
    }
    R->end = (uint32_t)voices(t);
    snprintf(R->msg, sizeof R->msg, "case %u: %s %s MIX %d TUNE %d DEC %d DRV %d TONE %d DUCK %d ATK %d SLICE %d BPM %d",
             c, ENGINES[t->engine]->name, ENGINES[t->engine]->presets[t->preset % ENGINES[t->engine]->npresets].name,
             t->p[P_RB_MIX], t->p[P_RB_TUNE], t->p[P_RB_DECAY], t->p[P_RB_DRIVE], t->p[P_RB_TONE], t->p[P_RB_DUCK],
             t->p[P_RB_ATK], t->p[P_RB_SLICE], song.g[G_BPM]);
}

/* the rumble alone of kit k at RUMBLE mix (the rest at their ends): out into R->y (3 s) */
static uint32_t A3, A4;                           /* (loud: the kit's own values (1), the track's DRIVE, LEVEL) */
static void loud(uint32_t k, int16_t mix)
{
    track_t *t = &trk[0];
    int32_t y[CTL];
    uint32_t b, i;
    setup(t, kit_e(k), kit_pi(k));
    if (A3) {                                        /* (the kit as it is, RUMBLE 0 or its own, the track pushed) */
        if (!mix)
            t->p[P_RB_MIX] = 0;
        t->p[P_DIST] = (int16_t)A4;
        t->p[P_LEVEL] = 127;
        transport_req = 1;
        for (b = 0; b < FS * 3u / CTL; b++) {
            blk(y);
            for (i = 0; i < CTL; i++)
                R->y[b * CTL + i] = (float)y[i];
        }
        return;
    }
    t->p[P_RB_MIX] = mix; t->p[P_RB_DRIVE] = 127; t->p[P_RB_TONE] = 127;
    t->p[P_DIST] = 90; t->p[P_LEVEL] = 80;           /* (the track loud, under the master's limiter: its gain riding
                                                       * on the rumble would move the kit's own highs) */
    transport_req = 1;
    for (b = 0; b < FS * 3u / CTL; b++) {
        blk(y);
        for (i = 0; i < CTL; i++)
            R->y[b * CTL + i] = (float)y[i];
    }
}

static void run(void (*f)(void))
{
    pid_t p;
    memset(R, 0, sizeof *R);
    if (!(p = fork())) {
        f();
        _exit(0);
    }
    waitpid(p, 0, 0);
}
static uint32_t A0, A1, A2;
static void do_sweep(void) { sweep(A0, (int)A1, (int)A2); }
static void do_fuzz(void) { fuzz(A0, A1); }
static void do_loud(void) { loud(A0, (int16_t)A1); }

/* energy of x (n) above hz, and in all (a DFT on a coarse grid: 4 Hz bins of 1 s) */
static double band_share(const float *a, const float *b, uint32_t n, double hz)
{
    static double re[FS / 2], im[FS / 2];
    double hi = 0, all = 0;
    uint32_t f, i, len = FS, off = n - FS;
    for (f = 8; f < 8000u; f += 4u) {
        double w = 2 * M_PI * f / FS, c = 2 * cos(w), s1 = 0, s2 = 0, pw;
        for (i = 0; i < len; i++) {
            double h = 0.5 - 0.5 * cos(2 * M_PI * i / len), v = (a[off + i] - (b ? b[off + i] : 0)) * h, s0 = v + c * s1 - s2;
            s2 = s1;
            s1 = s0;
        }
        pw = s1 * s1 + s2 * s2 - c * s1 * s2;
        all += pw;
        if (f >= hz)
            hi += pw;
        (void)re; (void)im;
    }
    return 10 * log10(hi / (all + 1) + 1e-30);
}

/* the energy of x (n, its last second) between lo and hi Hz, dB */
static double band_db(const float *x, uint32_t n, double lo, double hi)
{
    double e = 0;
    uint32_t f, i, off = n - FS;
    for (f = (uint32_t)lo; f < (uint32_t)hi; f += 20u) {
        double w = 2 * M_PI * f / FS, c = 2 * cos(w), s1 = 0, s2 = 0;
        for (i = 0; i < FS; i++) {
            double h = 0.5 - 0.5 * cos(2 * M_PI * i / FS), s0 = x[off + i] * h + c * s1 - s2;
            s2 = s1;
            s1 = s0;
        }
        e += s1 * s1 + s2 * s2 - c * s1 * s2;
    }
    return 10 * log10(e + 1);
}

int main(int argc, char **argv)
{
    uint32_t n = argc > 1 ? (uint32_t)atoi(argv[1]) : 2000u, seed = argc > 2 ? (uint32_t)atoi(argv[2]) : 7u, k, c;
    uint32_t frozen = 0, sat = 0, tails = 0, errs = 0, fz = 0, ends = 0;
    char what[200];
    static float yl[FS * 3];
    R = mmap(0, sizeof *R, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0);

    /* R.TONE swept on every kit with a RUMBLE */
    for (k = 0; k < NELEM(RB_KITS); k++)
        for (A1 = 0; A1 < 2u; A1++)
            for (A2 = 0; A2 < 2u; A2++) {
                A0 = k;
                run(do_sweep);
                sat += R->sat > 0;
                tails += R->tail > 1.0 * FS || R->end;
                errs += R->err != 0;
                if (R->sat || R->tail > 1.0 * FS || R->end || R->err)
                    printf("rumble_fuzz:   sweep kit %u fast %u ends %u: sat %u tail %.0f voices %u err %d\n", k, A1, A2,
                           R->sat, R->tail, R->end, R->err);
            }
    snprintf(what, sizeof what, "R.TONE swept on the %u kits (slow, a step a block; as they are, the rest at their ends): "
             "%u frozen (held at a bound), %u not ending, %u out of range", (unsigned)NELEM(RB_KITS), sat, tails, errs);
    check(what, !sat && !tails && !errs);

    /* random cases */
    sat = tails = errs = 0;
    for (c = getenv("RBDBG") ? (uint32_t)atoi(getenv("RBDBG")) : 0u; c < n; c++) {
        A0 = c;
        A1 = seed;
        run(do_fuzz);
        sat += R->sat > 0;
        tails += R->tail > 1.0 * FS;
        ends += R->end != 0;
        errs += R->err == 1;
        fz += R->err == 2;
        if (R->sat || R->tail > 1.0 * FS || R->end || R->err)
            printf("rumble_fuzz:   %s: sat %u tail %.0f voices %u err %d\n", R->msg, R->sat, R->tail, R->end, R->err);
    }
    snprintf(what, sizeof what, "%u random cases (every value, turned, a kit load, STOP / PLAY): %u held at bounds, %u "
             "not ending, %u voices left, %u out of range, %u not cleared", n, sat, tails, ends, errs, fz);
    check(what, !sat && !tails && !ends && !errs && !fz);

    /* the metallic noise: the rumble alone, every kit at its loudest */
    {
        double worst = -200, worst4 = -200;
        uint32_t wk = 0;
        for (k = 0; k < NELEM(RB_KITS); k++) {
            double h1, h4;
            A0 = k;
            A1 = 1;
            run(do_loud);
            memcpy(yl, R->y, sizeof yl);
            A1 = 127;
            run(do_loud);
            h1 = band_share(R->y, yl, FS * 3u, 1000);
            h4 = band_share(R->y, yl, FS * 3u, 400);
            if (h1 > worst) {
                worst = h1;
                wk = k;
            }
            worst4 = h4 > worst4 ? h4 : worst4;
        }
        snprintf(what, sizeof what, "the rumble alone, every kit at RUMBLE R.DRIVE R.TONE 127, DRIVE 90, LEVEL 80: above 1 kHz %.1f dB "
                 "of it at most (under -42; kit %u), above 400 Hz %.1f dB (under -15)", worst, wk, worst4);
        check(what, worst < -42 && worst4 < -15);
    }
    {   /* the whole output, the kit as it is, the track at LEVEL 127 with DRIVE 30 and 90: the highs (2 .. 10 kHz) the
         * RUMBLE adds to the kit's own (RC2: up to +13 dB, its tail through the track's DRIVE and its own fizz) */
        double worst = -200, e0, e1;
        uint32_t wk = 0, dv;
        for (dv = 30; dv <= 90u; dv += 60u)
            for (k = 0; k < NELEM(RB_KITS); k++) {
                A0 = k; A3 = 1; A4 = dv;
                A1 = 0;
                run(do_loud);
                memcpy(yl, R->y, sizeof yl);
                e0 = band_db(yl, FS * 3u, 2000, 10000);
                A1 = 1;
                run(do_loud);
                e1 = band_db(R->y, FS * 3u, 2000, 10000);
                if (e1 - e0 > worst) {
                    worst = e1 - e0;
                    wk = k;
                }
            }
        A3 = 0;
        snprintf(what, sizeof what, "the kits as they are, LEVEL 127, DRIVE 30 / 90: the highs 2 .. 10 kHz the RUMBLE adds "
                 "%+.1f dB at most (kit %u; under +3)", worst, wk);
        check(what, worst < 3.0);
    }
    printf(bad ? "rumble_fuzz: %d FAILED\n" : "rumble_fuzz: all ok\n", bad);
    return bad != 0;
}
