/* SPDX-License-Identifier: GPL-3.0-only */
/* (techno-presets-v2) The redundancy audit of the factory presets. Every preset of every engine is played by the
 * firmware's own engines, voices and FX (tests/hostsim.c, as tools/preset_bench.c), the same input for all:
 *   NOTE    C3 (48) at velocity 100, held 1.2 s, then 1.8 s of tail
 *   PHRASE  2 bars at 145 BPM: 1/8 notes over two octaves (C2 .. C4, short and tied), then a C minor 7 chord held a bar
 * and each render is reduced to a fingerprint:
 *   SPEC    the average power spectrum (NOTE + PHRASE) in 32 bands, 30 Hz .. 18 kHz (log spaced), in dB, the level
 *           taken out (each spectrum relative to its own total), floor -60 dB
 *   CENT    the spectral centroid, in octaves
 *   ENVN    the NOTE's level over time (10 ms frames to 300 ms, then 50 ms frames to 3 s), dB re its loudest, floor -50
 *   ENVP    the PHRASE's level over time (20 ms frames), dB re its loudest, floor -50 (articulation, release, tails)
 *   MOVE    the timbre's movement while the NOTE is held: per octave band, the spread over time of its share of the
 *           frame's energy (filter sweeps, LFOs, PWM), dB
 *   AMOD    the level's movement while the NOTE is held: the mean step between 10 ms frames, 300 .. 1200 ms, dB
 *           (tremolo, gates, a chopping LFO)
 *   STER    the side / mid energy of the PHRASE, dB (floor -40)
 *   LEVEL   the PHRASE's RMS, dB (weighed lightly: two presets that differ only in level are the same sound)
 * The distance of two presets: the root mean square of the components, each divided by what is a plain, audible
 * difference (SPEC 4 dB rms, CENT 1/2 octave, ENVN 6 dB, ENVP 6 dB, MOVE 2 dB, AMOD 1 dB, STER 6 dB, LEVEL 12 dB;
 * SPEC counts twice).
 * The threshold comes from the presets themselves: each one is rendered again with its tone knob (the engine's
 * CUTOFF, or what plays that part: BRIGHT, DCW, MOD LVL, TONE, COLOR) moved by 16 of 127, and the distance of every
 * preset to that twin is measured; a pair closer than the median of those twins is closer than half the presets are
 * to themselves with the cutoff turned by 1/8 of its travel (REDONDANCE.md). A second check reads the presets' values: two presets of one
 * engine whose values differ in nothing but the tone knob, the sends and small amounts (8 of 127 or less) are the
 * same patch whatever they sound like.
 *   cc -O2 -w -Ibuild/gen -Ifirmware/src -o build/host/preset_audit tools/preset_audit.c -lm
 *   build/host/preset_audit REPORT.md [FEATURES.tsv]
 * Prints the pairs under the threshold; REPORT.md lists them (and the groups they form), FEATURES.tsv each preset's
 * fingerprint summary (centroid, level, stereo, attack, length). */
#define main hostsim_main
#include "../tests/hostsim.c"
#undef main

#define BPM 145.0
#define NB 32                                     /* spectrum bands */
#define NFFT 2048
#define ENVN_N 84
#define MAXP 1024
#define MAXF (FS * 8)

typedef struct {
    float spec[NB];
    float cent;
    float envn[ENVN_N];
    float envp[400];
    uint32_t nenvp;
    float move, amod, ster, level;
    float attack_ms, len_ms;                      /* (for the report) to -20 dB from the start; the NOTE's -40 dB end */
} fp_t;

typedef struct {
    uint16_t e, pi;
    const preset_t *p;
    fp_t f, twin;
    int has_twin;
} item_t;

static item_t it[MAXP];
static uint32_t nit;
static float bufl[MAXF], bufr[MAXF];

/* ------------------------------------------------------------------------------------------------- the inputs --- */
typedef struct { double t; uint8_t on, note, vel; } aev_t;
static aev_t aev[256];
static uint32_t naev;
static double step_s(void) { return 60.0 / BPM / 4.0; }
static void ev_note(double t0, double len, uint32_t n, uint32_t vel)
{
    aev[naev++] = (aev_t){t0, 1, (uint8_t)n, (uint8_t)vel};
    aev[naev++] = (aev_t){t0 + len, 0, (uint8_t)n, 0};
}
static int aev_cmp(const void *a, const void *b)
{
    const aev_t *x = a, *y = b;
    if (x->t != y->t)
        return x->t < y->t ? -1 : 1;
    return (int)x->on - (int)y->on;
}
static double input_note(void)
{
    naev = 0;
    ev_note(0, 1.2, 48, 100);
    return 3.0;
}
static double input_phrase(void)
{
    static const uint8_t N[16] = {36, 43, 48, 38, 42, 55, 46, 60, 36, 48, 43, 51, 38, 58, 46, 53};
    static const uint8_t C[4] = {48, 55, 58, 63};
    uint32_t i;
    naev = 0;
    for (i = 0; i < 16u; i++)                     /* bar 1: 1/8s, every other one held into the next (legato) */
        ev_note(i * step_s(), i % 2u ? 0.5 * step_s() : 1.2 * step_s(), N[i], i % 4u ? 90 : 120);
    for (i = 16; i < 24u; i++)                    /* bar 2, first half: 1/8s */
        ev_note(i * 2.0 * step_s() - 16 * step_s(), 1.0 * step_s(), N[i - 8u], 100);
    for (i = 0; i < 4u; i++)                      /* bar 2, second half and on: the chord */
        ev_note(24 * step_s(), 8 * step_s(), C[i], 100);
    return 32 * step_s() + 2.0;
}

/* the tone knob: the parameter that plays "cutoff" on each engine (-1: none) */
static int tone_param(uint32_t e)
{
    static const struct { const char *n; int p; } T[] = {
        {"ANALOG", P_E4}, {"SUPERSAW", P_E2}, {"ACID", P_E1}, {"MINI", P_E0}, {"HOOVER", P_E4}, {"INDUS", P_IN_CUT},
        {"TRIO", P_E5}, {"NOISE", P_E2}, {"PHYS", P_E2}, {"GRAIN", P_E7}, {"FM6", P_E2}, {"PHASE", P_E2},
        {"DRUM", P_E2}, {"DRUM-X", P_E2}, {"SAMPLE", -1}};
    uint32_t i;
    for (i = 0; i < NELEM(T); i++)
        if (!strcmp(ENGINES[e]->name, T[i].n))
            return T[i].p;
    return -1;
}

/* p's render of the input into bufl/bufr; tweak: the tone knob moved by that much (0: as the preset loads) */
static uint32_t render(uint32_t e, uint32_t pi, double len, int tweak)
{
    uint32_t frames = (uint32_t)(len * FS) / CTL * CTL, f, k = 0, i;
    int32_t o[2 * CTL];
    track_t *t = &trk[0];
    if (frames > MAXF)
        frames = MAXF / CTL * CTL;
    qsort(aev, naev, sizeof aev[0], aev_cmp);
    host_tracks_init();
    song.g[G_BPM] = (int16_t)BPM;
    if (e == ENGI_DRUMX)                          /* (a preset with no kit of its own: the first factory kit, not the last one heard) */
        memcpy(dx_kit, DX_KITS[0], sizeof dx_kit);
    host_preset(t, e, pi);
    t->p[P_AMODE] = 0;
    if (tweak) {
        int tp = tone_param(e);
        if (tp >= 0) {
            const param_desc_t *d = tp >= P_E0 ? &ENGINES[e]->edit[tp - P_E0] : &TP[tp];
            int32_t v = t->p[tp], lo = d->min, hi = d->max;
            if (tp == P_IN_CUT && v < 0)
                v = 64;
            v = v + tweak > hi ? v - tweak : v + tweak;
            t->p[tp] = (int16_t)clamp(v, lo, hi);
        }
    }
    for (f = 0; f < frames; f += CTL) {
        double now = (double)f / FS;
        while (k < naev && aev[k].t <= now) {
            if (aev[k].on)
                input_on(t, aev[k].note, aev[k].vel);
            else
                input_off(t, aev[k].note);
            k++;
        }
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++) {
            bufl[f + i] = (float)o[2 * i] / 32768.0f;
            bufr[f + i] = (float)o[2 * i + 1] / 32768.0f;
        }
    }
    {   /* every voice let go: the next render starts from silence */
        uint32_t n;
        for (n = 0; n < 200u; n++)
            mix_block(o, CTL);
    }
    return frames;
}

/* --------------------------------------------------------------------------------------------- the analysis --- */
static void fft(float *re, float *im, uint32_t n)
{
    uint32_t i, j = 0, len, k;
    for (i = 1; i < n; i++) {
        uint32_t bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j) {
            float a = re[i];
            re[i] = re[j], re[j] = a;
            a = im[i], im[i] = im[j], im[j] = a;
        }
    }
    for (len = 2; len <= n; len <<= 1) {
        double ang = -2 * M_PI / len;
        for (i = 0; i < n; i += len)
            for (k = 0; k < len / 2; k++) {
                float wr = (float)cos(ang * k), wi = (float)sin(ang * k);
                float ur = re[i + k], ui = im[i + k];
                float vr = re[i + k + len / 2] * wr - im[i + k + len / 2] * wi;
                float vi = re[i + k + len / 2] * wi + im[i + k + len / 2] * wr;
                re[i + k] = ur + vr, im[i + k] = ui + vi;
                re[i + k + len / 2] = ur - vr, im[i + k + len / 2] = ui - vi;
            }
    }
}
static double band_lo(uint32_t b) { return 30.0 * pow(18000.0 / 30.0, (double)b / NB); }
/* adds the frames' band powers of the mono sum from a to b into acc (and, per frame, into fr[][8] octave bands) */
static uint32_t spectrum(uint32_t a, uint32_t b, double *acc, float (*fr)[8], uint32_t maxfr)
{
    static float re[NFFT], im[NFFT], win[NFFT];
    static int bandof[NFFT / 2];
    uint32_t s, i, nfr = 0;
    if (!win[1]) {
        for (i = 0; i < NFFT; i++)
            win[i] = (float)(0.5 - 0.5 * cos(2 * M_PI * i / NFFT));
        for (i = 0; i < NFFT / 2; i++) {
            double hz = (double)i * FS / NFFT;
            int k = -1;
            for (uint32_t q = 0; q < NB; q++)
                if (hz >= band_lo(q) && hz < band_lo(q + 1))
                    k = (int)q;
            bandof[i] = k;
        }
    }
    for (s = a; s + NFFT <= b; s += NFFT / 2) {
        double ob[8] = {0}, tot = 0;
        for (i = 0; i < NFFT; i++) {
            re[i] = 0.5f * (bufl[s + i] + bufr[s + i]) * win[i];
            im[i] = 0;
        }
        fft(re, im, NFFT);
        for (i = 1; i < NFFT / 2; i++) {
            double pw = (double)re[i] * re[i] + (double)im[i] * im[i];
            if (bandof[i] >= 0) {
                acc[bandof[i]] += pw;
                ob[bandof[i] / 4] += pw;
                tot += pw;
            }
        }
        if (fr && nfr < maxfr) {
            for (i = 0; i < 8u; i++)
                fr[nfr][i] = (float)(10 * log10((ob[i] + 1e-12 * (tot + 1e-9)) / (tot + 1e-9)));
            fr[nfr][0] = tot > 1e-9 ? fr[nfr][0] : -99.0f;   /* (a silent frame: marked) */
            nfr++;
        }
    }
    return nfr;
}
static float rms_db(uint32_t a, uint32_t b)
{
    double s = 0;
    uint32_t i;
    for (i = a; i < b; i++)
        s += (double)bufl[i] * bufl[i] + (double)bufr[i] * bufr[i];
    return (float)(10 * log10(s / (2.0 * (b > a ? b - a : 1)) + 1e-12));
}

static void fingerprint(uint32_t e, uint32_t pi, int tweak, fp_t *f)
{
    double acc[NB] = {0}, tot = 0, c = 0;
    static float fr[256][8];
    uint32_t n, i, b, nfr;
    float pk;
    memset(f, 0, sizeof *f);
    /* NOTE */
    n = render(e, pi, input_note(), tweak);
    nfr = spectrum(0, (uint32_t)(1.2 * FS), acc, fr, 256);
    spectrum((uint32_t)(1.2 * FS), n, acc, 0, 0);
    {
        float env[300];
        uint32_t ne = 0;
        pk = -200;
        for (i = 0; i < 300u && (i + 1u) * FS / 100u <= n; i++) {
            env[i] = rms_db(i * FS / 100u, (i + 1u) * FS / 100u);
            pk = env[i] > pk ? env[i] : pk;
        }
        ne = i;
        for (i = 0; i < ne; i++)
            env[i] = env[i] - pk < -50 ? -50 : env[i] - pk;
        for (i = 0; i < ENVN_N; i++) {
            uint32_t k = i < 30u ? i : 30u + (i - 30u) * 5u;
            f->envn[i] = k < ne ? env[k] : -50;
        }
        {   /* AMOD: the level's ups and downs while held (300 .. 1200 ms): tremolo, gates, choppy LFOs */
            double m = 0;
            uint32_t k, cnt = 0;
            for (k = 31; k < 120u && k < ne; k++, cnt++)
                m += fabs(env[k] - env[k - 1]);
            f->amod = cnt ? (float)(m / cnt) : 0;
        }
        f->attack_ms = 0;
        for (i = 0; i < ne && env[i] < -20; i++)
            ;
        f->attack_ms = (float)(i * 10);
        for (i = ne; i > 0 && env[i - 1] < -40; i--)
            ;
        f->len_ms = (float)(i * 10);
    }
    {   /* MOVE: the octave bands' share of the frame over the held part (from 100 ms) */
        double m = 0;
        uint32_t used = 0, k0 = (uint32_t)(0.1 * FS) / (NFFT / 2);
        for (b = 0; b < 8u; b++) {
            double s = 0, s2 = 0;
            uint32_t k, cnt = 0;
            for (k = k0; k < nfr; k++)
                if (fr[k][0] > -98.0f) {
                    float v = fr[k][b] < -40 ? -40 : fr[k][b];
                    s += v, s2 += (double)v * v, cnt++;
                }
            if (cnt > 2) {
                double mean = s / cnt, var = s2 / cnt - mean * mean;
                if (mean > -30) {                 /* (the bands that carry the sound) */
                    m += sqrt(var > 0 ? var : 0);
                    used++;
                }
            }
        }
        f->move = used ? (float)(m / used) : 0;
    }
    /* PHRASE */
    n = render(e, pi, input_phrase(), tweak);
    spectrum(0, n, acc, 0, 0);
    {
        double sm = 0, ss = 0;
        uint32_t k;
        pk = -200;
        f->nenvp = n / (FS / 50u);
        if (f->nenvp > NELEM(f->envp))
            f->nenvp = NELEM(f->envp);
        for (k = 0; k < f->nenvp; k++) {
            f->envp[k] = rms_db(k * (FS / 50u), (k + 1u) * (FS / 50u));
            pk = f->envp[k] > pk ? f->envp[k] : pk;
        }
        for (k = 0; k < f->nenvp; k++)
            f->envp[k] = f->envp[k] - pk < -50 ? -50 : f->envp[k] - pk;
        for (k = 0; k < n; k++) {
            double m = 0.5 * (bufl[k] + bufr[k]), s = 0.5 * (bufl[k] - bufr[k]);
            sm += m * m, ss += s * s;
        }
        f->ster = (float)(10 * log10((ss + 1e-12) / (sm + 1e-12)));
        if (f->ster < -40)
            f->ster = -40;
        f->level = rms_db(0, n);
    }
    for (b = 0; b < NB; b++)
        tot += acc[b];
    for (b = 0; b < NB; b++) {
        double v = 10 * log10((acc[b] + 1e-15) / (tot + 1e-15));
        f->spec[b] = (float)(v < -60 ? -60 : v);
        c += acc[b] / (tot + 1e-15) * log2(sqrt(band_lo(b) * band_lo(b + 1)));
    }
    f->cent = (float)c;
}

/* the components of the distance, each in "plain audible differences" */
typedef struct { double spec, cent, envn, envp, move, amod, ster, level, all; } dist_t;
static dist_t distance(const fp_t *a, const fp_t *b)
{
    dist_t d;
    double s = 0;
    uint32_t i, n = a->nenvp < b->nenvp ? a->nenvp : b->nenvp;
    for (i = 0; i < NB; i++)
        s += (double)(a->spec[i] - b->spec[i]) * (a->spec[i] - b->spec[i]);
    d.spec = sqrt(s / NB) / 4.0;
    d.cent = fabs(a->cent - b->cent) / 0.5;
    for (s = 0, i = 0; i < ENVN_N; i++)
        s += (double)(a->envn[i] - b->envn[i]) * (a->envn[i] - b->envn[i]);
    d.envn = sqrt(s / ENVN_N) / 6.0;
    for (s = 0, i = 0; i < n; i++)
        s += (double)(a->envp[i] - b->envp[i]) * (a->envp[i] - b->envp[i]);
    d.envp = n ? sqrt(s / n) / 6.0 : 0;
    d.move = fabs(a->move - b->move) / 2.0;
    d.amod = fabs(a->amod - b->amod) / 1.0;
    d.ster = fabs(a->ster - b->ster) / 6.0;
    d.level = fabs(a->level - b->level) / 12.0;
    d.all = sqrt((d.spec * d.spec * 2 + d.cent * d.cent + d.envn * d.envn + d.envp * d.envp + d.move * d.move +
                  d.amod * d.amod + d.ster * d.ster + d.level * d.level) / 9.0);
    return d;
}

/* the values check: the fields of a and b (one engine) that differ by more than 8 (an enum: at all), the tone knob
 * and the sends left out; 0 = the same patch */
static int fields_apart(uint32_t e, const preset_t *a, const preset_t *b, char *why, size_t nw)
{
    int n = 0, tp = tone_param(e);
    uint32_t i;
    why[0] = 0;
#define APART(cond, label)                                                                                         \
    do {                                                                                                           \
        if (cond) {                                                                                                \
            n++;                                                                                                   \
            if (strlen(why) + 12 < nw)                                                                             \
                strcat(why, label), strcat(why, " ");                                                              \
        }                                                                                                          \
    } while (0)
    for (i = 0; i < 8u; i++) {
        const param_desc_t *d = &ENGINES[e]->edit[i];
        int diff = abs(a->e[i] - b->e[i]);
        if ((int)(P_E0 + i) == tp)
            continue;
        if (e == ENGI_MINI && i == 7u) {          /* (MINI's SLOT: a patch, MINI_PSLOT's) */
            uint32_t ka = (uint32_t)(a - ENGINES[e]->presets), kb = (uint32_t)(b - ENGINES[e]->presets);
            diff = MINI_PSLOT[ka].slot != MINI_PSLOT[kb].slot;
        }
        APART(d->names || (e == ENGI_MINI && i == 7u) ? diff != 0 : diff > 8, d->label);
    }
    for (i = 0; i < 4u; i++) {
        static const char *const EN[4] = {"ATK", "DEC", "SUS", "REL"};
        APART(abs(a->env[i] - b->env[i]) > 8, EN[i]);
    }
    APART(abs(a->fenv - b->fenv) > 8, "FENV");
    APART(a->mono != b->mono, "MONO");
    if (e == ENGI_DRUMX) {                        /* (DRUM-X: its kit) */
        uint32_t ka = (uint32_t)(a - ENGINES[e]->presets), kb = (uint32_t)(b - ENGINES[e]->presets);
        APART(DRUMX_PKIT[ka] != DRUMX_PKIT[kb], "KIT");
    }
    APART(a->fx[0] && b->fx[0] && abs(a->fx[0] - b->fx[0]) > 24, "DIST");
    {   /* the extra values: a value one has and the other not (or another) */
        uint32_t j;
        for (i = 0; i < 4u && a->more[i].id; i++) {
            if (a->more[i].id == tp)
                continue;
            for (j = 0; j < 4u && b->more[j].id && b->more[j].id != a->more[i].id; j++)
                ;
            APART(j == 4u || !b->more[j].id || abs(a->more[i].v - b->more[j].v) > 8, "MORE");
        }
        for (i = 0; i < 4u && b->more[i].id; i++) {
            if (b->more[i].id == tp)
                continue;
            for (j = 0; j < 4u && a->more[j].id && a->more[j].id != b->more[i].id; j++)
                ;
            APART(j == 4u || !a->more[j].id, "MORE");
        }
    }
#undef APART
    return n;
}

static int dcmp(const void *a, const void *b)
{
    double x = *(const double *)a, y = *(const double *)b;
    return x < y ? -1 : x > y;
}
typedef struct { uint32_t a, b; dist_t d; int fields; char why[96]; } pair_t;
static pair_t pairs[200000];
static int pcmp(const void *a, const void *b)
{
    const pair_t *x = a, *y = b;
    return x->d.all < y->d.all ? -1 : x->d.all > y->d.all;
}
static uint32_t uf[MAXP];
static uint32_t uf_find(uint32_t x) { return uf[x] == x ? x : (uf[x] = uf_find(uf[x])); }

int main(int argc, char **argv)
{
    uint32_t e, pi, i, j, ntw = 0, np = 0, nflag = 0;
    static double tw[MAXP];
    double thr, p50, p90, p10;
    FILE *rep, *ft = 0;
    if (argc < 2) {
        fprintf(stderr, "usage: preset_audit REPORT.md [FEATURES.tsv]\n");
        return 2;
    }
    for (e = 0; e < NENGINES; e++)
        for (pi = 0; eng_ok(e) && pi < ENGINES[e]->npresets && nit < MAXP; pi++) {
            item_t *x = &it[nit++];
            if (ENGINES[e]->presets == SMP_PRESET_TABLE ||   /* (SAMPLE: the recorded instruments; not a synth bank) */
                (e == ENGI_DRUMX && !strcmp(ENGINES[e]->presets[pi].name, "OWN KIT"))) {   /* (the user's own kit) */
                nit--;
                continue;
            }
            x->e = (uint16_t)e, x->pi = (uint16_t)pi, x->p = &ENGINES[e]->presets[pi];
            fingerprint(e, pi, 0, &x->f);
            if (tone_param(e) >= 0) {
                fingerprint(e, pi, 16, &x->twin);
                x->has_twin = 1;
                tw[ntw++] = distance(&x->f, &x->twin).all;
            }
            fprintf(stderr, "\r%3u %-8s %-14s", nit, ENGINES[e]->name, x->p->name);
        }
    fprintf(stderr, "\n");
    qsort(tw, ntw, sizeof tw[0], dcmp);
    p10 = tw[ntw / 10], p50 = tw[ntw / 2], p90 = tw[ntw * 9 / 10];
    thr = getenv("THR") ? atof(getenv("THR")) : p50;   /* (REDONDANCE.md: the median twin of the bank at the start,
                                                   * 0.48 with AMOD, given as THR from then on so the bar does not move) */
    for (i = 0; i < nit; i++)
        uf[i] = i;
    for (i = 0; i < nit; i++)
        for (j = i + 1; j < nit; j++) {
            dist_t d = distance(&it[i].f, &it[j].f);
            char why[96] = "";
            int fa = it[i].e == it[j].e ? fields_apart(it[i].e, it[i].p, it[j].p, why, sizeof why) : 99;
            if ((d.all < thr || fa == 0) && np < NELEM(pairs)) {
                pairs[np].a = i, pairs[np].b = j, pairs[np].d = d, pairs[np].fields = fa;
                snprintf(pairs[np].why, sizeof pairs[np].why, "%s", why);
                np++;
                uf[uf_find(i)] = uf_find(j);
            }
        }
    qsort(pairs, np, sizeof pairs[0], pcmp);
    rep = fopen(argv[1], "w");
    if (!rep) {
        perror(argv[1]);
        return 1;
    }
    fprintf(rep, "<!-- written by tools/preset_audit.c -->\n");
    fprintf(rep, "presets: %u ; twins (tone knob +16): %u ; twin distance p10 %.3f, median %.3f, p90 %.3f ; "
                 "THRESHOLD %.3f\n\n", nit, ntw, p10, p50, p90, thr);
    fprintf(rep, "| # | preset A | preset B | distance | spec | cent | envN | envP | move | amod | ster | level | values |\n");
    fprintf(rep, "|---|---|---|---|---|---|---|---|---|---|---|---|---|\n");
    for (i = 0; i < np; i++) {
        const item_t *a = &it[pairs[i].a], *b = &it[pairs[i].b];
        const dist_t *d = &pairs[i].d;
        char vals[128];
        if (pairs[i].fields == 99)
            snprintf(vals, sizeof vals, "other engines");
        else if (pairs[i].fields == 0)
            snprintf(vals, sizeof vals, "**SAME PATCH** (tone knob / sends / small amounts only)");
        else
            snprintf(vals, sizeof vals, "%d apart: %s", pairs[i].fields, pairs[i].why);
        fprintf(rep, "| %u | %s %u %s | %s %u %s | **%.3f** | %.2f | %.2f | %.2f | %.2f | %.2f | %.2f | %.2f | %.2f | %s |\n",
                i + 1, ENGINES[a->e]->name, a->pi, a->p->name, ENGINES[b->e]->name, b->pi, b->p->name, d->all,
                d->spec, d->cent, d->envn, d->envp, d->move, d->amod, d->ster, d->level, vals);
        nflag++;
    }
    fprintf(rep, "\n## groups\n\n");
    for (i = 0; i < nit; i++) {
        uint32_t n = 0;
        if (uf_find(i) != i)
            continue;
        for (j = 0; j < nit; j++)
            n += uf_find(j) == i;
        if (n < 2)
            continue;
        fprintf(rep, "-");
        for (j = 0; j < nit; j++)
            if (uf_find(j) == i)
                fprintf(rep, " %s %u %s ;", ENGINES[it[j].e]->name, it[j].pi, it[j].p->name);
        fprintf(rep, "\n");
    }
    fclose(rep);
    if (argc > 2 && (ft = fopen(argv[2], "w"))) {
        fprintf(ft, "engine\tpi\tname\tcentroid_oct\tlevel_db\tstereo_db\tmove_db\tattack_ms\tlen_ms\ttwin\n");
        for (i = 0; i < nit; i++)
            fprintf(ft, "%s\t%u\t%s\t%.2f\t%.1f\t%.1f\t%.2f\t%.0f\t%.0f\t%.3f\n", ENGINES[it[i].e]->name, it[i].pi,
                    it[i].p->name, it[i].f.cent, it[i].f.level, it[i].f.ster, it[i].f.move, it[i].f.attack_ms,
                    it[i].f.len_ms, it[i].has_twin ? distance(&it[i].f, &it[i].twin).all : -1.0);
        fclose(ft);
    }
    printf("preset_audit: %u presets, threshold %.3f (twins p10 %.3f median %.3f p90 %.3f), %u pairs flagged\n", nit,
           thr, p10, p50, p90, nflag);
    return 0;
}
