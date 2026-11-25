/* SPDX-License-Identifier: GPL-3.0-only */
/* (Gir, rumble) the kick's RUMBLE of DRUM and DRUM-X (rumble.c, core.h P_RB_MIX .. P_RB_DUCK):
 *   - RUMBLE 0: nothing runs; whatever the other R. values, the kit bit for bit as with them at their defaults, and a
 *     classic kit (STD, 80, 10, 66, 55, 77, DEEP ..) loads with RUMBLE 0; every KT RMB kit with it on;
 *   - on: a tail between the kicks (the rumble alone: the render with it minus the one without, the sends at 0), ducked
 *     under each kick (just after it far below the middle of the beat), whatever FX > DUCK;
 *   - each of its six values changes what comes out (RUMBLE, R.TUNE, R.DECAY, R.DRIVE, R.TONE, R.DUCK);
 *   - R.TUNE: the tail at the note's harmonics (A: 55 110 165 Hz) above the frequencies between them, and moved by the
 *     note (D#: 77.8 155.6 Hz);
 *   - the kick lane's voice stays on while the tail sounds, and ends after it;
 *   - DRUM-X's kick and a light track's (T5) too; no sample out of range;
 *   - the values stored by id (sid_put / sid_get); a record from before them leaves RUMBLE 0.
 *   cc -O2 -w -Ibuild/gen -Ifirmware/src -o build/host/rumble_test tests/rumble_test.c -lm */
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

static int bad;
static void check(const char *what, int ok)
{
    printf("rumble: %-96s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}

#define BPM 150
#define NF (2u * 4u * FS * 60u / BPM / CTL * CTL)      /* two bars */
static int16_t sw_now;                                 /* (the swing of the R.SLICE checks) */
static int32_t bpm_now = BPM;                         /* (R.SLICE's check: another tempo too) */
static uint32_t beat(void) { return bpm_quarter((uint32_t)bpm_now); }

static uint32_t find(uint32_t e, const char *name)
{
    uint32_t k;
    for (k = 0; k < ENGINES[e]->npresets; k++)
        if (!strcmp(ENGINES[e]->presets[k].name, name))
            return k;
    printf("rumble: no preset %s\n", name);
    exit(1);
}

/* a kick on every beat for two bars on track tr (engine e, preset name), the sends at 0; tweak(t) sets values; the
 * left output into x (NF) */
static void kicks_here(uint32_t tr, uint32_t e, const char *name, void (*tweak)(track_t *), int32_t *x);
static void kicks(uint32_t tr, uint32_t e, const char *name, void (*tweak)(track_t *), int32_t *x)
{                                                      /* each render in a process of its own: from power-on, the */
    static int32_t *sh;                                /* same every time (no state left by the one before) */
    pid_t pid;
    int st;
    if (!sh)
        sh = mmap(0, NF * sizeof *sh, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0);
    pid = fork();
    if (!pid) {
        kicks_here(tr, e, name, tweak, sh);
        _exit(0);
    }
    waitpid(pid, &st, 0);
    memcpy(x, sh, NF * sizeof *sh);
}
static void kicks_here(uint32_t tr, uint32_t e, const char *name, void (*tweak)(track_t *), int32_t *x)
{
    track_t *t = &trk[tr];
    int32_t o[2 * CTL];
    uint32_t i, f;
    host_tracks_init();
    song.g[G_BPM] = (int16_t)bpm_now;
    host_preset(t, e, find(e, name));
    t->p[P_AMODE] = 0;
    t->p[P_SLEN] = 16;
    t->p[P_SDIV] = 2;
    t->p[P_DIST] = t->p[P_CHOR] = t->p[P_DLY] = t->p[P_REV] = 0;
    t->p[P_LEVEL] = 80;
    if (tweak)
        tweak(t);
    for (i = 0; i < 16u; i++) {
        uint8_t k = 36;
        put_step(t, i, i % 4u == 0u, &k, i % 4u == 0u ? ST_NOTE : ST_REST, 0);
    }
    transport_req = 1;
    for (f = 0; f < NF; f += CTL) {
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++)
            x[f + i] = o[2 * i];
    }
}
static double rms(const int32_t *x, uint32_t a, uint32_t b)
{
    double s = 0;
    uint32_t i;
    for (i = a; i < b; i++)
        s += (double)x[i] * x[i];
    return sqrt(s / (b > a ? b - a : 1));
}
/* the rumble alone: the kit with the values tweak sets minus the kit with RUMBLE 0 */
static int32_t K0[NF], K1[NF], D[NF], D2[NF];
static void rumble_of(uint32_t tr, uint32_t e, const char *name, void (*tweak)(track_t *), int32_t *d)
{
    uint32_t i;
    kicks(tr, e, name, tweak, K1);
    for (i = 0; i < NF; i++)
        d[i] = K1[i] - K0[i];
}
static void off(track_t *t) { t->p[P_RB_MIX] = 0; }
static void off_sw(track_t *t) { t->p[P_RB_MIX] = 0; if (sw_now) t->p[P_SSWING] = sw_now; }
static void off_wild(track_t *t)
{
    t->p[P_RB_MIX] = 0;
    t->p[P_RB_TUNE] = 3;
    t->p[P_RB_DECAY] = 127;
    t->p[P_RB_DRIVE] = 127;
    t->p[P_RB_TONE] = 0;
    t->p[P_RB_DUCK] = 0;
}
static int16_t tv[8];                                  /* the values set by `set` (a < 0: as the kit has it) */
static void set(track_t *t)
{
    uint32_t i;
    for (i = 0; i < 8u; i++)
        if (tv[i] != -100)
            t->p[P_RB_MIX + i] = tv[i];
    if (sw_now)
        t->p[P_SSWING] = sw_now;
}
static void tv_kit(void)
{
    uint32_t i;
    for (i = 0; i < 8u; i++)
        tv[i] = -100;
}
/* the power at hz over the second bar (Goertzel) */
static double power_at(const int32_t *x, double hz)
{
    double w = 2 * M_PI * hz / FS, c = 2 * cos(w), s0, s1 = 0, s2 = 0;
    uint32_t i;
    for (i = NF / 2u; i < NF; i++) {
        s0 = x[i] + c * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    return s1 * s1 + s2 * s2 - c * s1 * s2;
}
static double harm_ratio_db(const int32_t *x, double f0)
{
    double on = 0, between = 0;
    uint32_t h;
    for (h = 1; h <= 3u; h++) {
        on += power_at(x, f0 * h);
        between += power_at(x, f0 * (h + 0.5));
    }
    return 10 * log10(on / (between + 1));
}
/* the share of the beat just after each kick (0 .. 20 ms) and the beat's middle (150 .. 330 ms), second bar */
static void duck_windows(const int32_t *d, double *after, double *mid)
{
    uint32_t b, B = beat(), s0 = 0, s1 = 0;
    double a = 0, m = 0;
    for (b = 4; b < 8u; b++) {
        uint32_t k = b * B;
        double r = rms(d, k + FS / 1000u, k + FS / 50u), q = rms(d, k + FS * 15u / 100u, k + FS * 33u / 100u);
        a += r * r;
        m += q * q;
        s0++;
        s1++;
    }
    *after = sqrt(a / s0);
    *mid = sqrt(m / s1);
}
/* the 40 .. 200 Hz band's level between the kicks (150 .. 380 ms of each beat, the second bar on), dB */
static double low_mid_db(const int32_t *x)
{
    double a = 1 - exp(-2 * M_PI * 200 / FS), b = 1 - exp(-2 * M_PI * 40 / FS), y = 0, y2 = 0, l = 0, e = 0;
    uint32_t i, c = 0, B = beat();
    for (i = 0; i < NF; i++) {
        double v;
        y += a * (x[i] - y);
        y2 += a * (y - y2);
        l += b * (y2 - l);
        v = y2 - l;
        if (i >= 4u * B && i % B >= FS * 15u / 100u && i % B < FS * 38u / 100u) {
            e += v * v;
            c++;
        }
    }
    return 10 * log10(e / c + 1);
}
static double dist(const int32_t *a, const int32_t *b)   /* how far apart two rumbles are, relative */
{
    double s = 0, n = 0;
    uint32_t i;
    for (i = 0; i < NF; i++) {
        double d = (double)a[i] - b[i];
        s += d * d;
        n += (double)a[i] * a[i] + (double)b[i] * b[i];
    }
    return sqrt(2 * s / (n + 1));
}

int main(void)
{
    uint32_t i, kp, same;
    double after, mid, rk, rr;
    char what[160];


    /* off: bit for bit */
    kicks(0, ENGI_DRUM, "KT HARD", off, K0);
    kicks(0, ENGI_DRUM, "KT HARD", off, K1);
    for (same = 1, i = 0; i < NF; i++)
        same &= K0[i] == K1[i];
    check("twice the same render: the same (each in a process of its own)", same);
    kicks(0, ENGI_DRUM, "KT HARD", off_wild, K1);
    for (same = 1, i = 0; i < NF; i++)
        same &= K0[i] == K1[i];
    check("RUMBLE 0: the other R. values change nothing (bit for bit)", same);
    {
        static const char *const CLASSIC[] = {"DRUM KIT", "WAR DRUMS", "BOOM KIT", "RHYTHM BOX", "ELECTRO KIT", "STUDIO KIT",
                                              "DEEP DRUMS", "SNAP KIT", "PERC KIT", "KT DETROIT", "KT MINIMAL", "KT DUB"};
        static const char *const CLASSIC_X[] = {"OWN KIT", "HADRON KIT", "FACTORY KIT", "DEEP KIT", "INDUSTRIAL", "SOFT KIT",
                                                "TRIBAL KIT", "LASER KIT"};
        int ok = 1, on = 1, nrmb = 0, nkt = 0;
        for (i = 0; i < NELEM(CLASSIC); i++) {
            host_preset(&trk[0], ENGI_DRUM, find(ENGI_DRUM, CLASSIC[i]));
            ok &= trk[0].p[P_RB_MIX] == 0;
        }
        for (i = 0; i < NELEM(CLASSIC_X); i++) {
            host_preset(&trk[0], ENGI_DRUMX, find(ENGI_DRUMX, CLASSIC_X[i]));
            ok &= trk[0].p[P_RB_MIX] == 0;
        }
        check("the classic kits (DRUM's STD 80 10 66 55 77 DEEP .., DRUM-X's own) load with RUMBLE 0", ok);
        for (kp = 0; kp < 2u; kp++) {
            uint32_t e = kp ? ENGI_DRUMX : ENGI_DRUM;
            for (i = 0; i < ENGINES[e]->npresets; i++) {
                host_preset(&trk[0], e, i);
                if (!strncmp(ENGINES[e]->presets[i].name, "KT RMB", 6)) {
                    on &= trk[0].p[P_RB_MIX] > 0 && trk[0].p[P_RB_TUNE] == 9;
                    nrmb++;
                }
                nkt += trk[0].p[P_RB_MIX] > 0;
            }
        }
        snprintf(what, sizeof what, "the %d KT RMB kits with a RUMBLE tuned to A; %d kits with a RUMBLE in all", nrmb, nkt);
        check(what, on && nrmb == 7);
        for (i = 0; i < NELEM(RB_KITS); i++) {       /* every entry names a kit */
            uint32_t e = RB_KITS[i][0] >= 128u ? ENGI_DRUMX : ENGI_DRUM, pi = RB_KITS[i][0] & 127u;
            ok &= pi < ENGINES[e]->npresets && !strncmp(ENGINES[e]->presets[pi].name, "KT ", 3);
        }
        check("every RB_KITS entry is a KT kit of its engine", ok);
    }

    /* on: the rumble alone, between the kicks, ducked */
    tv_kit();
    rumble_of(0, ENGI_DRUM, "KT HARD", set, D);
    rk = rms(K0, NF / 2u, NF);
    rr = rms(D, NF / 2u, NF);
    snprintf(what, sizeof what, "KT HARD: a rumble %.1f dB under the kick (between -20 and +3)", 20 * log10(rr / rk));
    check(what, rr > rk * 0.1 && rr < rk * 1.41);
    {
        double lo0 = low_mid_db(K0), lo1 = low_mid_db(K1);
        snprintf(what, sizeof what, "its phase: the low between the kicks %.1f dB up with it (it adds to the kick's tail, "
                 "not hollows it; at least 3)", lo1 - lo0);
        check(what, lo1 > lo0 + 3);
    }
    duck_windows(D, &after, &mid);
    snprintf(what, sizeof what, "ducked: 1 .. 20 ms after each kick %.1f dB under the beat's middle (at least 12)",
             20 * log10(mid / (after + 1)));
    check(what, after * 4 < mid);
    tv_kit();
    tv[5] = 0;
    rumble_of(0, ENGI_DRUM, "KT HARD", set, D2);
    {
        double a2, m2;
        duck_windows(D2, &a2, &m2);
        snprintf(what, sizeof what, "R.DUCK 0: no duck (after / middle %.2f, ducked %.2f)", a2 / m2, after / mid);
        check(what, a2 / m2 > 2 * after / mid);
    }
    trk[0].p[P_DKSRC] = 0;
    check("the duck is the engine's own (FX > DUCK OFF)", trk[0].p[P_DKSRC] == 0 && after * 4 < mid);

    /* each value moves it */
    {
        static const struct { const char *name; uint32_t k; int16_t a, b; } KN[] = {
            {"RUMBLE 40 / 110", 0, 40, 110}, {"R.TUNE OFF / A", 1, -1, 9}, {"R.TUNE A / D#", 1, 9, 3},
            {"R.DECAY 20 / 120", 2, 20, 120}, {"R.DRIVE 0 / 127", 3, 0, 127}, {"R.TONE 10 / 120", 4, 10, 120},
            {"R.DUCK 10 / 127", 5, 10, 127},
        };
        for (i = 0; i < NELEM(KN); i++) {
            double d;
            tv_kit();
            tv[KN[i].k] = KN[i].a;
            rumble_of(0, ENGI_DRUM, "KT HARD", set, D);
            tv[KN[i].k] = KN[i].b;
            rumble_of(0, ENGI_DRUM, "KT HARD", set, D2);
            d = dist(D, D2);
            snprintf(what, sizeof what, "%s: the rumble %.0f %% apart (at least 20)", KN[i].name, 100 * d);
            check(what, d > 0.2);
        }
    }

    /* tuned */
    tv_kit();
    tv[1] = 9;
    tv[3] = 20;
    rumble_of(0, ENGI_DRUM, "KT HARD", set, D);
    tv[1] = 3;
    rumble_of(0, ENGI_DRUM, "KT HARD", set, D2);
    {
        double pa = power_at(D, 55) + power_at(D, 110), pd = power_at(D, 77.78) + power_at(D, 155.56);
        double qa = power_at(D2, 55) + power_at(D2, 110), qd = power_at(D2, 77.78) + power_at(D2, 155.56);
        double a_a = harm_ratio_db(D, 55.0);
        snprintf(what, sizeof what, "R.TUNE A: 55 110 Hz %.1f dB over D#'s 77.8 155.6 (D#: %.1f the other way); A's "
                 "harmonics %.1f dB over between", 10 * log10(pa / pd), 10 * log10(qd / qa), a_a);
        check(what, pa > 4 * pd && qd > 4 * qa && a_a > 6);
    }

    /* the tail: one kick, the voice on while it sounds, then off */
    {
        track_t *t = &trk[0];
        int32_t o[2 * CTL];
        uint32_t f, nv = 0, end = 0;
        double tail = 0, kick = 0;
        host_tracks_init();
        host_preset(t, ENGI_DRUM, find(ENGI_DRUM, "KT RMB DEEP"));
        t->p[P_DIST] = t->p[P_CHOR] = t->p[P_DLY] = t->p[P_REV] = 0;
        t->p[P_RB_DUCK] = 0;
        input_on(t, 36, 110);
        for (f = 0; f < 10u * FS; f += CTL) {
            if (f == CTL * 8u)
                input_off(t, 36);
            mix_block(o, CTL);
            for (i = 0; i < CTL; i++) {
                double s = (double)o[2 * i] * o[2 * i];
                if (f < FS / 10u)
                    kick += s;
                if (f >= FS * 6u / 10u && f < FS * 9u / 10u)
                    tail += s;
            }
            for (nv = 0, i = 0; i < NVOICE; i++)
                nv += t->v[i].active;
            if (!nv && !end)
                end = f;
        }
        snprintf(what, sizeof what, "KT RMB DEEP, one kick: 600 .. 900 ms after it %.0f dB under its first 100 ms; the "
                 "voice off at %.2f s", 10 * log10(tail / 3 / kick), end / (double)FS);
        check(what, tail > kick * 3e-5 && end > FS * 6u / 10u && end < 8u * FS);
    }

    /* DRUM-X, a light track */
    kicks(0, ENGI_DRUMX, "KT DX HARD", off, K0);
    tv_kit();
    rumble_of(0, ENGI_DRUMX, "KT DX HARD", set, D);
    rr = rms(D, NF / 2u, NF);
    rk = rms(K0, NF / 2u, NF);
    snprintf(what, sizeof what, "DRUM-X KT DX HARD: a rumble %.1f dB under the kick", 20 * log10(rr / rk));
    check(what, rr > rk * 0.1 && rr < rk * 1.41);
    kicks(4, ENGI_DRUM, "KT RMB HARD", off, K0);
    tv_kit();
    rumble_of(4, ENGI_DRUM, "KT RMB HARD", set, D);
    rr = rms(D, NF / 2u, NF);
    rk = rms(K0, NF / 2u, NF);
    snprintf(what, sizeof what, "T5 (a light track) KT RMB HARD: a rumble %.1f dB under the kick", 20 * log10(rr / rk));
    check(what, rr > rk * 0.1 && rr < rk * 1.41);
    {                                                  /* every kit with its RUMBLE: in range */
        int32_t mx = 0;
        for (kp = 0; kp < NELEM(RB_KITS); kp++) {
            uint32_t e = RB_KITS[kp][0] >= 128u ? ENGI_DRUMX : ENGI_DRUM;
            tv_kit();
            tv[0] = 127;
            tv[3] = 127;
            kicks(0, e, ENGINES[e]->presets[RB_KITS[kp][0] & 127u].name, set, K1);
            for (i = 0; i < NF; i++)
                mx = K1[i] > mx ? K1[i] : -K1[i] > mx ? -K1[i] : mx;
        }
        snprintf(what, sizeof what, "every rumble kit at RUMBLE and R.DRIVE 127: the peak %d (the output's range 32767)", mx);
        check(what, mx <= 32767);
    }

    /* (Gir 1.0) R.SLICE: each pattern's pulses on the beat's 16ths (the kick's ducked), at 150 and 128 BPM, straight and
     * swung (the odd 16ths later, as the sequencer's); R.ATTACK; no click */
    {
        static const char *const SN[5] = {"CLASSIC", "OFFBEAT", "DOUBLE", "ROLL", "GATE"};
        static const uint8_t ON[5] = {0, 4u, 2u | 8u, 2u | 4u | 8u, 2u | 4u | 8u};
        int16_t bpms[2] = {150, 128}, sws[2] = {0, 40};
        uint32_t mode, bi, si, okg = 1, okt = 1, okc = 1;
        double worst_ratio = 1e9, worst_dt = 0, worst_step = 0;
        for (mode = 1; mode < 5u; mode++)
            for (bi = 0; bi < 2u; bi++)
                for (si = 0; si < 2u; si++) {
                    uint32_t B, st, b, j, i, sw;
                    double on = 0, off = 0, mx = 0, step = 0;
                    uint32_t non = 0, noff = 0;
                    bpm_now = bpms[bi];
                    tv_kit();
                    tv[7 - 0] = (int16_t)mode;          /* (tv: RUMBLE .. R.SLICE in P_* order) */
                    tv[5] = 0;                          /* (no duck: the slicer alone shapes it) */
                    sw_now = sws[si];
                    kicks(0, ENGI_DRUM, "KT RMB HARD", off_sw, K0);
                    rumble_of(0, ENGI_DRUM, "KT RMB HARD", set, D);
                    B = beat();
                    st = B / 4u;
                    sw = (uint32_t)(sws[si] * (int32_t)st / 250);
                    for (b = 2; b < 7u && (b + 1u) * B < NF; b++)
                        for (j = 1; j < 4u; j++) {
                            uint32_t beg = b * B + j * st + ((j & 1u) ? sw : 0u), end = j < 3u ? b * B + (j + 1u) * st +
                                           (((j + 1u) & 1u) ? sw : 0u) : (b + 1u) * B, len = (end - beg) * 7u / 8u, k;
                            double e = 0, pk = 0;
                            for (i = beg + len / 4u; i < beg + len * 3u / 4u; i++)
                                e += (double)D[i] * D[i];
                            e /= len / 2u;
                            if ((ON[mode] >> j) & 1u) {
                                on += e, non++;
                                for (i = beg; i < beg + len; i++)
                                    pk = fabs((double)D[i]) > pk ? fabs((double)D[i]) : pk;
                                for (k = beg > FS / 100u ? beg - FS / 100u : 0u; k < beg + len; k++)   /* its onset */
                                    if (fabs((double)D[k]) > pk * 0.3)
                                        break;
                                {
                                    double dt = ((double)k - beg) * 1000.0 / FS;
                                    worst_dt = fabs(dt) > fabs(worst_dt) ? dt : worst_dt;
                                    okt &= dt > -1.0 && dt < 4.0;
                                }
                            } else {
                                off += e, noff++;
                            }
                        }
                    for (i = B; i < NF; i++) {
                        double a = fabs((double)D[i]), dd = fabs((double)D[i] - D[i - 1]);
                        mx = a > mx ? a : mx;
                        step = dd > step ? dd : step;
                    }
                    if (noff) {
                        double r = 10 * log10((on / non + 1) / (off / noff + 1));
                        worst_ratio = r < worst_ratio ? r : worst_ratio;
                        okg &= r > 15;
                    }
                    worst_step = step / (mx + 1) > worst_step ? step / (mx + 1) : worst_step;
                    okc &= step < mx * 0.15;
                    if (!okg || !okt || !okc)
                        printf("rumble:   %s at %d BPM swing %d: on/off %.1f dB, onset %.2f ms, step %.3f of the peak\n",
                               SN[mode], bpms[bi], sws[si], noff ? 10 * log10((on / non + 1) / (off / noff + 1)) : 99.0,
                               worst_dt, step / (mx + 1));
                }
        bpm_now = BPM;
        sw_now = 0;
        snprintf(what, sizeof what, "R.SLICE OFFBEAT DOUBLE ROLL GATE at 150 and 128 BPM, straight and swung 40: the pulses on "
                 "their 16ths, %.1f dB over the gaps (15)", worst_ratio);
        check(what, okg);
        snprintf(what, sizeof what, "  each pulse starts on its 16th (swung: the odd ones later): within %.2f ms (-1 .. 4)", worst_dt);
        check(what, okt);
        snprintf(what, sizeof what, "  no click: the largest step a sample %.3f of the peak (under 0.15)", worst_step);
        check(what, okc);
    }
    {   /* R.ATTACK: CLASSIC's ramp from the kick (64: ~76 ms), a pulse's rise (127: most of it) */
        static int32_t D0[NF];
        uint32_t B = beat(), a = 64u * 64u * 13230u / 16129u + 1u, b, i;
        double q1 = 0, q1n = 0, q3 = 0, q3n = 0;
        kicks(0, ENGI_DRUM, "KT RMB HARD", off, K0);   /* (the reference at 150 BPM, straight again) */
        tv_kit();
        tv[5] = 0;
        rumble_of(0, ENGI_DRUM, "KT RMB HARD", set, D0);
        tv[6] = 64;
        rumble_of(0, ENGI_DRUM, "KT RMB HARD", set, D);
        for (b = 2; b < 4u; b++)
            for (i = 0; i < B; i++) {
                double x0 = (double)D0[b * B + i] * D0[b * B + i], x1 = (double)D[b * B + i] * D[b * B + i];
                if (i > a / 5u && i < a / 3u)
                    q1 += x1, q1n += x0;
                if (i > a * 3u / 2u && i < a * 2u)
                    q3 += x1, q3n += x0;
            }
        snprintf(what, sizeof what, "R.ATTACK 64 (CLASSIC, a %.0f ms ramp): at a quarter of it %.2f of the level, past it %.2f",
                 a * 1000.0 / FS, sqrt(q1 / (q1n + 1)), sqrt(q3 / (q3n + 1)));
        check(what, sqrt(q1 / (q1n + 1)) > 0.15 && sqrt(q1 / (q1n + 1)) < 0.4 && sqrt(q3 / (q3n + 1)) > 0.9 &&
                    sqrt(q3 / (q3n + 1)) < 1.1);
        tv_kit();
        tv[5] = 0;
        tv[7] = 2;                                       /* DOUBLE */
        rumble_of(0, ENGI_DRUM, "KT RMB HARD", set, D0);
        tv[6] = 127;
        rumble_of(0, ENGI_DRUM, "KT RMB HARD", set, D);
        q1 = q1n = 0;
        for (b = 2; b < 4u; b++)
            for (i = B / 4u; i < B / 4u + B / 16u; i++)   /* the first quarter of the first pulse */
                q1 += (double)D[b * B + i] * D[b * B + i], q1n += (double)D0[b * B + i] * D0[b * B + i];
        snprintf(what, sizeof what, "R.ATTACK 127 (DOUBLE): a pulse's first quarter at %.2f of its level at 0 (a slow rise)",
                 sqrt(q1 / (q1n + 1)));
        check(what, sqrt(q1 / (q1n + 1)) < 0.5);
    }

    /* stored */
    {
        uint8_t b[1u + SID_COUNT];
        int16_t p[P_COUNT];
        int ok;
        for (i = 0; i < P_COUNT; i++)
            p[i] = TP[i].def;
        p[P_RB_MIX] = 77;
        p[P_RB_TUNE] = -1;
        p[P_RB_DUCK] = 99;
        ok = sid_put(b, p) == 1u + SID_COUNT;
        for (i = 0; i < P_COUNT; i++)
            p[i] = TP[i].def;
        ok &= sid_get(p, b, sizeof b) == 1u + SID_COUNT && p[P_RB_MIX] == 77 && p[P_RB_TUNE] == -1 && p[P_RB_DUCK] == 99;
        check("stored by id: RUMBLE 77, R.TUNE OFF, R.DUCK 99 back", ok && p_sid(P_RB_MIX) == 170u && p_sid(P_RB_DUCK) == 175u);
        b[0] = 170;                                    /* a record of before: SIDs 0..169 */
        for (i = 0; i < P_COUNT; i++)
            p[i] = TP[i].def;
        ok = sid_get(p, b, sizeof b) == 171u && p[P_RB_MIX] == 0;
        check("a record from before the RUMBLE: RUMBLE 0", ok);
    }
    printf(bad ? "rumble: %d FAILED\n" : "rumble: all ok\n", bad);
    return bad != 0;
}
