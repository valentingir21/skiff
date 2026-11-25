/* SPDX-License-Identifier: GPL-3.0-only */
/* (Gir 1.0) the kicks of DRUM and DRUM-X, measured: one kick of each kit (C2, velocity 110, RUMBLE 0, the track's FX
 * as the kit has them, its level at 100), from power-on in a process of its own:
 *   click   the 2 .. 6 kHz band's energy in the first 15 ms, dB under the kick's whole energy in its first 100 ms
 *   crest   the peak over the RMS of the first 50 ms, dB (the attack's punch)
 *   dive    the time the low band's pitch (zero crossings of the 20 .. 400 Hz band) takes to fall within 1.3x of where it
 *           ends (at 80 .. 120 ms), ms; and how far it falls (octaves, from its first 5 ms)
 *   punch   the first 10 ms's RMS over the body's (30 .. 80 ms), dB: an attack above its body
 *   tail    the time to 40 dB under its peak (the envelope of 5 ms windows), ms
 *   cc -O2 -w -Ibuild/gen -Ifirmware/src -o build/host/kick_measure tools/kick_measure.c -lm
 *   build/host/kick_measure [WAVDIR]     (CSV on stdout: engine;kit;click;crest;dive;octaves;tail; WAVs of each kick)
 *   KC=.. KP=..: K.CLICK, K.PUNCH set on every kit (to try them); KIT=name: that kit only */
#define main hostsim_main
#include "../tests/hostsim.c"
#undef main
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#define NK (FS * 3u / 2u)
static float *Y;
static int keep_rumble;                               /* (kick_test: the kit's RUMBLE kept, at 150 BPM's kicks) */

static void one(uint32_t e, uint32_t pi)
{
    track_t *t = &trk[0];
    int32_t o[2 * CTL];
    uint32_t f, i;
    host_tracks_init();
    host_preset(t, e, pi);
    if (!keep_rumble)
        t->p[P_RB_MIX] = 0;
    t->p[P_CHOR] = t->p[P_DLY] = t->p[P_REV] = 0;
    if (getenv("KC"))                                  /* (KC=, KP=: K.CLICK, K.PUNCH on every kit, to try them) */
        t->p[P_KCLICK] = (int16_t)atoi(getenv("KC"));
    if (getenv("KP"))
        t->p[P_KPUNCH] = (int16_t)atoi(getenv("KP"));
    t->p[P_LEVEL] = 100;
    input_on(t, 36, 110);
    for (f = 0; f < NK; f += CTL) {
        if (f == CTL * 4u)
            input_off(t, 36);
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++)
            Y[f + i] = (float)o[2 * i];
    }
}

static double bandpass_energy(const float *x, uint32_t n, double lo, double hi)
{
    /* two biquads (RBJ band-pass, Q 0.7 at the geometric centre, twice) */
    double fc = sqrt(lo * hi), w = 2 * M_PI * fc / FS, al = sin(w) * sinh(log(2.0) / 2 * log2(hi / lo) * w / sin(w));
    double b0 = al, a0 = 1 + al, a1 = -2 * cos(w), a2 = 1 - al, e = 0, x1 = 0, x2 = 0, y1 = 0, y2 = 0, u1 = 0, u2 = 0, v1 = 0, v2 = 0;
    uint32_t i;
    for (i = 0; i < n; i++) {
        double y = (b0 * x[i] - b0 * x2 - a1 * y1 - a2 * y2) / a0, v = (b0 * y - b0 * u2 - a1 * v1 - a2 * v2) / a0;
        x2 = x1; x1 = x[i]; y2 = y1; y1 = y; u2 = u1; u1 = y; v2 = v1; v1 = v;
        e += v * v;
    }
    return e;
}

static double last_click, last_punch;
static void measure(const char *eng, const char *name, const float *x, FILE *csv)
{
    static float lp[NK];
    uint32_t i, n15 = FS * 15u / 1000u, n50 = FS / 20u, n100 = FS / 10u, pk_i = 0;
    double all = 0, pk = 0, rms50 = 0, click, crest, a = 1 - exp(-2 * M_PI * 400 / FS), b = 1 - exp(-2 * M_PI * 20 / FS);
    double y = 0, y2 = 0, l = 0, fend = 0, f0 = 0, dive = -1, tail = 0, env_pk = 0, punch_db = 0;
    uint32_t zc_last = 0, nz = 0;
    for (i = 0; i < n100; i++)
        all += (double)x[i] * x[i];
    click = 10 * log10(bandpass_energy(x, n15, 2000, 6000) / (all + 1) + 1e-12);
    for (i = 0; i < n50; i++) {
        rms50 += (double)x[i] * x[i];
        if (fabs(x[i]) > pk)
            pk = fabs(x[i]), pk_i = i;
    }
    crest = 20 * log10(pk / (sqrt(rms50 / n50) + 1e-9) + 1e-12);
    for (i = 0; i < NK; i++) {                       /* the low band: 20 .. 400 Hz */
        y += a * (x[i] - y);
        y2 += a * (y - y2);
        l += b * (y2 - l);
        lp[i] = (float)(y2 - l);
    }
    {                                                /* its pitch from the zero crossings (a half period each) */
        static double fr[NK];
        uint32_t nf = 0, k;
        double sum = 0;
        uint32_t c = 0;
        for (i = 1; i < NK && i < FS / 5u; i++)
            if ((lp[i - 1] < 0) != (lp[i] < 0)) {
                if (zc_last) {
                    fr[nf] = FS / (2.0 * (i - zc_last));
                    nf++;
                    fr[nf - 1] = fr[nf - 1];
                }
                zc_last = i;
                nz++;
                fr[NK - 1 - (nf ? nf - 1 : 0)] = i;  /* (the crossing's time) */
            }
        for (k = 0; k < nf; k++) {
            double tms = fr[NK - 1 - k] * 1000.0 / FS;
            if (tms >= 80 && tms <= 120)
                sum += fr[k], c++;
        }
        fend = c ? sum / c : (nf ? fr[nf - 1] : 0);
        f0 = nf > 1 ? (fr[0] + fr[1]) / 2 : fend;
        for (k = 0; k < nf; k++)
            if (fr[k] < fend * 1.3) {
                dive = fr[NK - 1 - k] * 1000.0 / FS;
                break;
            }
    }
    for (i = 0; i + 220u < NK; i += 220u) {         /* the tail: 5 ms windows */
        double e = 0;
        uint32_t j;
        for (j = 0; j < 220u; j++)
            e += (double)x[i + j] * x[i + j];
        e = sqrt(e / 220);
        if (e > env_pk)
            env_pk = e;
        else if (e > env_pk * 0.01)
            tail = (i + 220u) * 1000.0 / FS;
    }
    {                                                /* punch: the first 10 ms over the body (30 .. 80 ms) */
        double a10 = 0, b50 = 0;
        for (i = 0; i < FS / 100u; i++)
            a10 += (double)x[i] * x[i];
        for (i = FS * 3u / 100u; i < FS * 8u / 100u; i++)
            b50 += (double)x[i] * x[i];
        punch_db = 10 * log10((a10 / (FS / 100u) + 1) / (b50 / (FS / 20u) + 1));
    }
    last_click = click;
    last_punch = punch_db;
    if (csv)
        fprintf(csv, "%s;%s;%.1f;%.1f;%.1f;%.0f;%.2f;%.0f\n", eng, name, click, crest, punch_db, dive,
            f0 > 0 && fend > 0 ? log2(f0 / fend) : 0.0, tail);
    (void)pk_i;
}

#ifndef KICK_MAIN
#define KICK_MAIN main
#endif
int KICK_MAIN(int argc, char **argv)
{
    uint32_t e, k;
    Y = mmap(0, NK * sizeof *Y, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0);
    printf("engine;kit;click_dB;crest_dB;punch_dB;dive_ms;dive_oct;tail_ms\n");
    for (e = 0; e < NENGINES; e++) {
        if (e != ENGI_DRUM && e != ENGI_DRUMX)
            continue;
        for (k = 0; k < ENGINES[e]->npresets; k++) {
            pid_t p;
            if (getenv("KIT") && strcmp(getenv("KIT"), ENGINES[e]->presets[k].name))
                continue;
            p = fork();
            if (!p) {
                one(e, k);
                _exit(0);
            }
            waitpid(p, 0, 0);
            measure(ENGINES[e]->name, ENGINES[e]->presets[k].name, Y, stdout);
            if (argc > 1) {
                char path[600], *c;
                FILE *w;
                uint32_t i;
                snprintf(path, sizeof path, "%s/kick_%s_%s.wav", argv[1], ENGINES[e]->name, ENGINES[e]->presets[k].name);
                for (c = path + strlen(argv[1]) + 1; *c; c++)
                    if (*c == ' ')
                        *c = '_';
                if ((w = fopen(path, "wb"))) {
                    wav_hdr(w, NK);
                    for (i = 0; i < NK; i++)
                        wav_put(w, (int32_t)Y[i], (int32_t)Y[i]);
                    fclose(w);
                }
            }
        }
    }
    return 0;
}
