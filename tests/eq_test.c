/* SPDX-License-Identifier: GPL-3.0-only */
/* (techno-mixer) the track's isolator and DEPTH (fx.c eq_*, core.h P_EQ_LOW .. P_DEPTH):
 *   - all bands at 0 dB and DEPTH 0: the mix bit for bit as without them (a whole song of four tracks);
 *   - the three bands at 0 dB with the isolator running: white noise comes out at the same level in every third of an
 *     octave (an allpass: within 0.3 dB);
 *   - KILL: each band's tone down by 30 dB or more (LOW at 50 Hz, MID at 1 kHz: 20 dB, HIGH at 10 kHz), the others'
 *     tones kept; +6 dB: up by 6 dB (within 0.5 dB);
 *   - no click: a held tone while a band is swept from KILL to +6 dB and back every block, killed at once, and the
 *     isolator switched on and off: no sample-to-sample step larger than the tone's own (x 1.3);
 *   - DEPTH: the level down, the REVERB send up, the top down; 0: nothing;
 *   - the values stored by id (core.h sid_put / sid_get) and kept over a sound load; a record from before them leaves
 *     64 64 64 0.
 *   cc -O2 -w -Ibuild/gen -Ifirmware/src -o build/host/eq_test tests/eq_test.c -lm */
#define main hostsim_main
#include "hostsim.c"
#undef main

static int bad;
static void check(const char *what, int ok)
{
    printf("eq: %-100s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}

/* the isolator alone (eq_run) on signal x: band values lo mi hi (P_EQ_*), fully wet, from fresh states */
static void eq_alone(int32_t *x, uint32_t n, int lo, int mi, int hi)
{
    eq_trk_t q;
    int32_t g[3];
    uint32_t i;
    track_t t;
    memset(&q, 0, sizeof q);
    memset(&t, 0, sizeof t);
    t.p[P_EQ_LOW] = (int16_t)lo;
    t.p[P_EQ_MID] = (int16_t)mi;
    t.p[P_EQ_HIGH] = (int16_t)hi;
    eq_gains(&t, g);
    for (i = 0; i < 3u; i++)
        q.g[i] = g[i];
    for (i = 0; i < n; i += CTL)
        eq_run(&q, 0, x + i, CTL, 32768, 32768, g);
}
static double rms(const int32_t *x, uint32_t a, uint32_t b)
{
    double s = 0;
    uint32_t i;
    for (i = a; i < b; i++)
        s += (double)x[i] * x[i];
    return sqrt(s / (b - a));
}
static double tone_db(double hz, int lo, int mi, int hi)
{
    static int32_t x[FS];
    uint32_t i;
    double r0;
    for (i = 0; i < FS; i++)
        x[i] = (int32_t)(16000 * sin(2 * M_PI * hz * i / FS));
    r0 = rms(x, FS / 2, FS);
    eq_alone(x, FS, lo, mi, hi);
    return 20 * log10(rms(x, FS / 2, FS) / r0);
}

/* a held tone on track 0 (ANALOG's first preset), n blocks; act(block) may turn the values; the largest step between
 * samples of the left output after the attack */
static int32_t held_steps(void (*act)(track_t *, uint32_t), uint32_t blocks)
{
    track_t *t = &trk[0];
    int32_t o[2 * CTL], last = 0, mx = 0;
    uint32_t b, i;
    host_tracks_init();
    host_preset(t, 0, 0);
    t->p[P_AMODE] = 0;
    t->p[P_DIST] = t->p[P_CHOR] = t->p[P_DLY] = t->p[P_REV] = 0;
    t->p[P_ATK] = 0;
    t->p[P_SUS] = 127;
    input_on(t, 45, 100);
    for (b = 0; b < blocks; b++) {
        if (act)
            act(t, b);
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++) {
            int32_t d = o[2 * i] - last;
            last = o[2 * i];
            if (b > 40u && (d < 0 ? -d : d) > mx)
                mx = d < 0 ? -d : d;
        }
    }
    return mx;
}
static void act_sweep(track_t *t, uint32_t b) { t->p[P_EQ_LOW] = (int16_t)(b % 2u ? 0 : 127); t->p[P_EQ_HIGH] = (int16_t)(b % 3u ? 127 : 0); }
static void act_kill(track_t *t, uint32_t b) { t->p[P_EQ_LOW] = (int16_t)((b / 50u) % 2u ? 0 : 64); }
static void act_switch(track_t *t, uint32_t b) { t->p[P_EQ_MID] = (int16_t)((b / 20u) % 2u ? 66 : 64); }

int main(void)
{
    uint32_t i;
    /* 1. bit for bit: with every value as it was (64 64 64 0) the isolator never runs (no state, no fade, the signal
     * untouched); every golden render (tests/golden.txt, regress.c: every preset, the mixes) is unchanged by it */
    {
        int32_t o[2 * CTL];
        uint32_t f, any = 0;
        host_tracks_init();
        host_preset(&trk[0], 0, 0);
        host_preset(&trk[1], ENGI_FM6, 0);
        host_drums(&trk[3]);
        for (i = 0; i < 4u; i++)
            trk[i].p[P_AMODE] = 0;
        input_on(&trk[0], 45, 100);
        input_on(&trk[1], 60, 100);
        trk_note_on(&trk[3], 36, 100);
        for (f = 0; f < FS; f += CTL)
            mix_block(o, CTL);
        for (i = 0; i < NTRK; i++)
            any |= eq_trk[i].on | eq_trk[i].fade | (uint32_t)eq_trk[i].s[0].a0.y1;
        check("all bands at 0 dB, DEPTH 0: the isolator never runs (the signal bit for bit; the golden renders unchanged)", !any);
    }
    /* 2. flat: white noise through the three bands at 0 dB (running), by thirds of an octave */
    {
        static int32_t x[4 * FS], y[4 * FS];
        int32_t st = 0x1234567;
        double worst = 0;
        uint32_t k;
        for (i = 0; i < 4u * FS; i++)
            x[i] = y[i] = (int32_t)(noise32(&st) >> 18) - 8192;
        eq_alone(y, 4u * FS, 64, 64, 64);
        for (k = 0; k < 28u; k++) {                      /* 40 Hz .. 16 kHz: a band-pass of each third, both signals */
            double fc = 40.0 * pow(2.0, k / 3.0), w = 2 * M_PI * fc / FS, al = sin(w) / (2 * 4.3), a0 = 1 + al;
            double b0 = al / a0, a1 = -2 * cos(w) / a0, a2 = (1 - al) / a0, ex = 0, ey = 0;
            double x1 = 0, x2 = 0, y1 = 0, y2 = 0, u1 = 0, u2 = 0, v1 = 0, v2 = 0;
            for (i = 0; i < 4u * FS; i++) {
                double p = b0 * x[i] - b0 * x2 - a1 * y1 - a2 * y2, q = b0 * y[i] - b0 * u2 - a1 * v1 - a2 * v2;
                x2 = x1; x1 = x[i]; y2 = y1; y1 = p;
                u2 = u1; u1 = y[i]; v2 = v1; v1 = q;
                if (i > FS / 2) {
                    ex += p * p;
                    ey += q * q;
                }
            }
            if (fabs(10 * log10(ey / ex)) > worst)
                worst = fabs(10 * log10(ey / ex));
        }
        printf("eq:   flat: the largest third-octave difference %.2f dB\n", worst);
        check("three bands at 0 dB: the same level in every third of an octave, 40 Hz .. 16 kHz (within 0.3 dB)", worst < 0.3);
    }
    /* 3. KILL and +6 dB */
    {
        double l = tone_db(50, 0, 64, 64), m = tone_db(1000, 64, 0, 64), h = tone_db(10000, 64, 64, 0);
        double l2 = tone_db(1000, 0, 64, 64), m2 = tone_db(50, 64, 0, 64), h2 = tone_db(1000, 64, 64, 0);
        double lu = tone_db(50, 127, 64, 64), mu = tone_db(1000, 64, 127, 64), hu = tone_db(10000, 64, 64, 127);
        printf("eq:   KILL: LOW at 50 Hz %.1f dB, MID at 1 kHz %.1f dB, HIGH at 10 kHz %.1f dB; the others' tones %.2f %.2f %.2f dB\n",
               l, m, h, l2, m2, h2);
        printf("eq:   +6 dB: LOW at 50 Hz %+.2f, MID at 1 kHz %+.2f, HIGH at 10 kHz %+.2f dB\n", lu, mu, hu);
        check("KILL: LOW 50 Hz, HIGH 10 kHz under -30 dB, MID 1 kHz under -20 dB; the other bands' tones within 0.5 dB",
              l < -30 && h < -30 && m < -20 && fabs(l2) < 0.5 && fabs(m2) < 0.5 && fabs(h2) < 0.5);
        check("+6 dB: each band's tone up 6 dB (within 0.5)", fabs(lu - 6) < 0.5 && fabs(mu - 6) < 0.5 && fabs(hu - 6) < 0.5);
        check("the curve: 0 KILL, 64 0 dB, 127 +6 dB, rising all the way", EQ_G[0] == 0 && EQ_G[64] == 16384 &&
              EQ_DB10[127] == 60 && EQ_DB10[64] == 0);
        for (i = 1; i < 128u; i++)
            if (EQ_G[i] < EQ_G[i - 1])
                break;
        check("  .. never falling", i == 128u);
    }
    /* 4. no click */
    {
        int32_t base = held_steps(0, 300), sw = held_steps(act_sweep, 300), kl = held_steps(act_kill, 300),
                sn = held_steps(act_switch, 300);
        printf("eq:   largest step: the tone alone %d, swept every block %d, killed at once %d, switched on and off %d\n",
               base, sw, kl, sn);
        check("no click: swept KILL <-> +6 dB every block, a band killed at once, the isolator switched on and off "
              "(each step at most 1.3 x the tone's own)", sw <= base * 13 / 10 && kl <= base * 13 / 10 && sn <= base * 13 / 10);
    }
    /* 5. DEPTH */
    {
        int32_t lvl = 4096, r = 0, lvl2 = 4096, r2 = 0, g[3];
        track_t t;
        memset(&t, 0, sizeof t);
        t.p[P_EQ_LOW] = t.p[P_EQ_MID] = t.p[P_EQ_HIGH] = 64;
        eq_depth(&t, &lvl2, &r2);
        t.p[P_DEPTH] = 127;
        eq_depth(&t, &lvl, &r);
        eq_gains(&t, g);
        check("DEPTH 127: level -6 dB, the REVERB send from 0 to 60 %, HIGH -12 dB; DEPTH 0: none of it",
              abs(lvl - 2048) < 8 && abs(r - 19660) < 200 && abs(g[2] - 4096) < 48 && lvl2 == 4096 && r2 == 0 &&
              g[0] == 16384 && g[1] == 16384);
    }
    /* 6. stored, kept */
    {
        static int16_t p[P_COUNT], q[P_COUNT];
        static uint8_t rec[1u + SID_COUNT];
        uint32_t n;
        for (i = 0; i < P_COUNT; i++)
            p[i] = TP[i].def;
        p[P_EQ_LOW] = 0;
        p[P_EQ_MID] = 100;
        p[P_EQ_HIGH] = 20;
        p[P_DEPTH] = 90;
        n = sid_put(rec, p);
        for (i = 0; i < P_COUNT; i++)
            q[i] = TP[i].def;
        (void)sid_get(q, rec, n);
        check("LOW MID HIGH DEPTH stored by id (SKF4, user sounds) and read back",
              n && q[P_EQ_LOW] == 0 && q[P_EQ_MID] == 100 && q[P_EQ_HIGH] == 20 && q[P_DEPTH] == 90);
        rec[0] = (uint8_t)p_sid(P_EQ_LOW);               /* a record from before them: its count stops short */
        for (i = 0; i < P_COUNT; i++)
            q[i] = TP[i].def;
        (void)sid_get(q, rec, 1u + rec[0]);
        check("  a record from before them: 64 64 64 0 (as it sounded)",
              q[P_EQ_LOW] == 64 && q[P_EQ_MID] == 64 && q[P_EQ_HIGH] == 64 && q[P_DEPTH] == 0);
        check("  kept over a sound load (the mix's, as LEVEL and PAN)", param_kept(P_EQ_LOW) && param_kept(P_DEPTH));
    }
    printf("%s\n", bad ? "EQ TEST FAILED" : "eq: all passed");
    return bad ? 1 : 0;
}
