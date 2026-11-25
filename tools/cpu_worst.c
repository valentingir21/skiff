/* SPDX-License-Identifier: GPL-3.0-only */
/* (techno-engines) CPU bench: what each engine costs per voice, and the worst realistic case of the six tracks on two
 * cores, as a share of one FM-1 core. Runs the firmware's own engines, voices, mix and FX (tests/hostsim.c, the second
 * core as tests/c1_host.h's thread, held to "no split" so that one thread counts everything and the 16-voice budget of
 * 2 CORES applies), counting the Mac's instructions per sample (proc_pid_rusage, as tests/regress.c).
 *   cc -O2 -w -Ibuild/gen -Ifirmware/src -pthread -o build/host/cpu_worst tools/cpu_worst.c -lm
 *   C1_MASK=0 build/host/cpu_worst [ENGINE ..]     (default: every engine; ENGINE: its name, the worst case with it)
 * Host instructions are turned into FM-1 cycles with one measured point: a PHYS MODAL voice struck costs ~13 % of a
 * core on the device (tests/phys_test.c). One core = 360 MHz / 44.1 kHz = 8163 cycles a sample. An estimate, the same
 * way for every engine: compare engines with it, and keep the margin.
 * Per engine: its heaviest preset (8 notes asked, the engine's cap), cost over the idle mix, per voice.
 * Worst case with ENGINE: T1 ENGINE's heaviest preset at its cap (asked last: the budget steals from the others), the
 * five others the heaviest of PHASE, GRAIN, PHYS, FM6 and DRUM-X (8 notes or lanes each), every send at 100, REVERB
 * HALL, DELAY PING, CHORUS PHASER; the voices that sound (16 at most) counted, then each track alone with that many,
 * split as fx.c mix_split does (the bus effects and master on core 0) -> the busier core's load. */
#include "../tests/c1_host.h"
#define main hostsim_main
#include "../tests/hostsim.c"
#undef main
#ifdef __APPLE__
#include <libproc.h>
#include <sys/resource.h>
#endif

#define CORE_CYC (360e6 / 44100.0)
static double cyc_per_instr = 0;                   /* the calibration (PHYS: the pessimistic one) */
static double cyc_idle = 0;                        /* the other: the idle mix at 14 % (SMK37.md, v1.3.2 on the same SoC) */

static uint64_t instr_now(void)
{
#ifdef __APPLE__
    struct rusage_info_v4 ri;
    if (!proc_pid_rusage(getpid(), RUSAGE_INFO_V4, (rusage_info_t *)&ri))
        return ri.ri_instructions;
#endif
    return 0;
}

typedef struct { uint8_t e, pi, n; } part_t;       /* engine, preset, notes (0: silent) */
static int fx_all;                                  /* every send at 100, the heavy bus models */
static int eq_on;                                   /* ((techno-mixer) the worst cases' runs: EQ_ALL applies (not the calibration) */

static void setup(const part_t *P)
{
    static const uint8_t NOTES[8] = {48, 52, 55, 59, 60, 64, 67, 71};
    static const uint8_t KIT[8] = {36, 38, 39, 42, 46, 45, 37, 56};
    uint32_t p, i, order[NPART] = {1, 2, 3, 4, 5, 0};   /* T1 (the engine looked at) last */
    host_tracks_init();
    if (fx_all) {
        song.g[G_RTYPE] = 2;
        song.g[G_DMODE] = DM_PING;
        song.g[G_CMODE] = 2;
    }
    for (p = 0; p < NPART; p++) {
        host_preset(&trk[p], P[p].e, P[p].pi);
        trk[p].p[P_VOICE] = V_POLY;
        trk[p].p[P_SUS] = 127;
        trk[p].p[P_AMODE] = 0;
        trk[p].p[P_DIST] = trk[p].p[P_CHOR] = trk[p].p[P_DLY] = trk[p].p[P_REV] = fx_all ? 100 : 0;   /* (alone: none) */
        if (eq_on && getenv("EQ_ALL")) {                         /* ((techno-mixer) EQ_ALL=1: every track's isolator running (a
                                                         * track's own cost: in its runs alone too) */
            trk[p].p[P_EQ_LOW] = 30;
            trk[p].p[P_EQ_MID] = 80;
            trk[p].p[P_EQ_HIGH] = 100;
            trk[p].p[P_DEPTH] = 40;
        }
    }
    for (i = 0; i < NPART; i++) {
        p = order[i];
        for (uint32_t k = 0; k < P[p].n; k++)
            trk_note_on(&trk[p], ENGINES[P[p].e]->oneshot ? KIT[k] : NOTES[k] + 12u * (p % 3u), 100);
    }
}

/* instructions a sample over 1 s (after 0.5 s); sounding voices per track at the end in nv. Each in a child of its
 * own (as tests/regress.c's jobs): nothing of one run (voices, the buses' tails) sounds into the next */
static double run_here(const part_t *P, uint32_t *nv);
static double run(const part_t *P, uint32_t *nv)
{
    int fd[2];
    pid_t pid;
    struct { double c; uint32_t nv[NPART]; } r = {0};
    if (pipe(fd))
        exit(1);
    pid = fork();
    if (!pid) {
        close(fd[0]);
        r.c = run_here(P, r.nv);
        if (write(fd[1], &r, sizeof r) != (ssize_t)sizeof r)
            _exit(1);
        _exit(0);
    }
    close(fd[1]);
    if (read(fd[0], &r, sizeof r) != (ssize_t)sizeof r) {
        fprintf(stderr, "cpu_worst: a run failed\n");
        exit(1);
    }
    close(fd[0]);
    waitpid(pid, 0, 0);
    if (nv)
        memcpy(nv, r.nv, sizeof r.nv);
    return r.c;
}
static double run_here(const part_t *P, uint32_t *nv)
{
    static const uint8_t KIT[8] = {36, 38, 39, 42, 46, 45, 37, 56};
    uint32_t nb = FS / CTL, k, p, i;
    uint64_t i0 = 0;
    int32_t out[2 * CTL];
    setup(P);
    for (k = 0; k < nb / 2u + nb; k++) {
        if (k == nb / 2u)
            i0 = instr_now();
        for (p = 0; p < NPART; p++)
            if (P[p].n && ENGINES[P[p].e]->oneshot && k % 64u == 63u)
                for (i = 0; i < P[p].n; i++)
                    trk_note_on(&trk[p], KIT[i], 100);
        mix_block(out, CTL);
    }
    if (nv)
        for (p = 0; p < NPART; p++) {
            nv[p] = 0;
            for (i = 0; i < NVOICE; i++)
                nv[p] += trk[p].v[i].active != 0;
        }
    return (double)(instr_now() - i0) / (nb * CTL);
}

static const part_t SILENT[NPART];
static double idle_cost(void)
{
    return run(SILENT, 0);
}

static uint32_t eng_by_name(const char *n)
{
    uint32_t e;
    for (e = 0; e < NENGINES; e++)
        if (eng_ok(e) && !strcmp(ENGINES[e]->name, n))
            return e;
    return 0xFF;
}

/* the heaviest preset of e (8 notes asked) and its cost over idle */
static uint32_t heaviest(uint32_t e, double idle, double *cost)
{
    uint32_t pi, best = 0;
    double hi = -1;
    for (pi = 0; pi < ENGINES[e]->npresets; pi++) {
        part_t P[NPART] = {{0}};
        double c;
        P[0] = (part_t){(uint8_t)e, (uint8_t)pi, 8};
        c = run(P, 0) - idle;
        if (c > hi) {
            hi = c;
            best = pi;
        }
    }
    *cost = hi;
    return best;
}

static double pct(double instr) { return 100.0 * instr * cyc_per_instr / CORE_CYC; }

/* scenario sc 0: every other track asks 8 notes (DRUM-X its 8 lanes: half the budget of 16); sc 1: the others 1 note
 * each (DRUM-X 4 lanes), so that x sounds at its cap. The tracks' own work measured alone without the sends (sum_t);
 * what the sends and the bus effects add (the whole mix with every send, less the tracks' work and the idle mix)
 * put on core 0 with the idle mix (conservative: a track's drive runs where the track does) */
static void worst(uint32_t x, uint32_t sc, const uint32_t *hp, double idle, double idle_fx)
{
    static const char *const OTHERS[5] = {"PHASE", "GRAIN", "PHYS", "FM6", "DRUM-X"};
    part_t P[NPART];
    double tot, solo[NPART], sum = 0, best = 1e9, c0 = 0, c1 = 0, fxw, busy;
    uint32_t k, m, bm = 0, nv[NPART], n1[NPART], v = 0, v1 = 0;
    P[0] = (part_t){(uint8_t)x, (uint8_t)hp[x], 8};
    for (k = 0; k < 5u; k++) {
        uint32_t o = eng_by_name(OTHERS[k]);
        P[k + 1] = (part_t){(uint8_t)o, (uint8_t)hp[o], (uint8_t)(sc ? (ENGINES[o]->oneshot ? 4 : 1) : 8)};
    }
    fx_all = 1;
    eq_on = 1;
    tot = run(P, nv);                                 /* everything, every send */
    c1h_off = 1;
    run(P, n1);                                       /* (1 CORE: how many sound) */
    c1h_off = 0;
    fx_all = 0;
    printf("\nworst case %c with %s (%s): sounding", 'A' + sc, ENGINES[x]->name, ENGINES[x]->presets[hp[x]].name);
    for (k = 0; k < NPART; k++) {
        printf(" %s %u", ENGINES[P[k].e]->name, nv[k]);
        v += nv[k];
        v1 += n1[k];
    }
    printf(" (%u voices in all; 1 CORE: %u, %s %u)\n", v, v1, ENGINES[x]->name, n1[0]);
    for (k = 0; k < NPART; k++) {
        part_t S[NPART] = {{0}};
        S[0] = P[k];
        S[0].n = (uint8_t)(ENGINES[P[k].e]->oneshot ? P[k].n : nv[k]);
        solo[k] = run(S, 0) - idle;
        sum += solo[k];
        if (getenv("CPU_DEBUG"))
            printf("    alone, no sends: %-8s %u notes %.0f\n", ENGINES[S[0].e]->name, S[0].n, solo[k]);
    }
    eq_on = 0;
    fxw = tot - sum - idle;                           /* the sends, the buses, the master: core 0 */
    best = 1e9;
    for (m = 1; m < (1u << NPART) - 1u; m++) {        /* fx.c mix_split: the busier side least */
        double s1 = 0, w;
        for (k = 0; k < NPART; k++)
            if (m >> k & 1u)
                s1 += solo[k];
#ifdef C0_OWN
        w = s1 > sum - s1 + C0_OWN ? s1 : sum - s1 + C0_OWN;   /* ((techno-mixer) core 0's own work counted, as fx.c */
#else
        w = s1 > sum - s1 ? s1 : sum - s1;
#endif
        if (w < best) {
            best = w;
            bm = m;
        }
    }
    for (k = 0; k < NPART; k++)
        if (bm >> k & 1u)
            c1 += solo[k];
        else
            c0 += solo[k];
    c0 += idle + fxw;
    busy = c0 > c1 ? c0 : c1;
    printf("  whole mix %.0f instr/sample: the tracks %.0f, idle %.0f, sends + bus effects %.0f\n", tot, sum, idle, fxw);
    printf("  2 CORES: core 0 %.0f %%, core 1 %.0f %% -> busier core %.0f %% (margin %.0f %%); by the idle point: %.0f %% "
           "(margin %.0f %%)\n", pct(c0), pct(c1), pct(busy), 100 - pct(busy), pct(busy) * cyc_idle / cyc_per_instr,
           100 - pct(busy) * cyc_idle / cyc_per_instr);
    printf("  1 CORE (8 voices): %.0f %% of the core%s\n", pct(sum * v1 / (v ? v : 1) + idle + fxw),
           pct(sum * v1 / (v ? v : 1) + idle + fxw) > 85 ? " -> over 85 %: audio.c sheds voices (a fade, no glitch)" : "");
}

int main(int argc, char **argv)
{
    uint32_t e, a, nv[NPART], hp[NENGINES];
    double idle, idle_fx, hc[NENGINES];
    setvbuf(stdout, 0, _IONBF, 0);
    c1h_off = 0;                                    /* 2 CORES: the budget of 16 (C1_MASK=0: no split, one thread) */
    if (!getenv("C1_MASK") || strcmp(getenv("C1_MASK"), "0")) {
        fprintf(stderr, "run with C1_MASK=0\n");
        return 2;
    }
    idle = idle_cost();
    {   /* the calibration: PHYS BELL TREE (MODAL, struck), one voice */
        part_t P[NPART] = {{0}};
        double c;
        P[0] = (part_t){ENGI_PHYS, 0, 1};
        c = run(P, 0) - idle;
        cyc_per_instr = 0.13 * CORE_CYC / c;
        printf("calibration: PHYS %s 1 voice = %.0f host instr / sample = 13 %% of a core -> %.2f cycles / instr\n",
               ENGINES[ENGI_PHYS]->presets[0].name, c, cyc_per_instr);
    }
    cyc_idle = 0.14 * CORE_CYC / idle;
    printf("idle mix: %.0f instr (%.1f %% of a core); the other point, the idle mix at 14 %%: %.2f cycles / instr\n\n",
           idle, pct(idle), cyc_idle);
    printf("%-9s %-13s %4s %8s %8s %8s %8s\n", "engine", "heaviest", "cap", "instr", "/voice", "%core", "%/voice");
    for (e = 0; e < NENGINES; e++) {
        part_t P[NPART] = {{0}};
        uint32_t n;
        if (!eng_ok(e))
            continue;
        hp[e] = heaviest(e, idle, &hc[e]);
        P[0] = (part_t){(uint8_t)e, (uint8_t)hp[e], 8};
        run(P, nv);
        n = nv[0] ? nv[0] : 1;
        printf("%-9s %-13s %4u %8.0f %8.0f %7.1f%% %7.1f%%\n", ENGINES[e]->name, ENGINES[e]->presets[hp[e]].name, nv[0],
               hc[e], hc[e] / n, pct(hc[e]), pct(hc[e] / n));
    }
    fx_all = 1;
    idle_fx = idle_cost();
    printf("\nidle mix with every send at 100, HALL, PING, PHASER: %.0f instr (%.1f %%)\n", idle_fx, pct(idle_fx));
    for (a = 1; a < (uint32_t)argc; a++) {
        uint32_t x = eng_by_name(argv[a]), sc;
        if (x == 0xFF) {
            printf("%s: no such engine in this build\n", argv[a]);
            continue;
        }
        for (sc = 0; sc < 2u; sc++)
            worst(x, sc, hp, idle, idle_fx);
    }
    return 0;
}
