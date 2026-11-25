/* SPDX-License-Identifier: GPL-3.0-only */
/* (Gir 1.0) the kicks that hit (EDIT > KICK, rumble.c kick_shape; tools/kick_measure.c's measures):
 *   - every techno kit (KT ..) of DRUM and DRUM-X: its click (2 .. 6 kHz in its first 15 ms, under its first 100 ms) at
 *     -30 dB or more (the hard ones: hard techno, schranz, gabber, indus, rave, hardgroove, tribe, warehouse) or -32 dB
 *     (techno, Berlin, Detroit, minimal, acid, dub, deep); before Gir 1.0's KICK page most DRUM kits were at -35 .. -61;
 *   - the classic kits (Skiff's, without KT) leave K.CLICK and K.PUNCH at 0: their kick as it was;
 *   - with its RUMBLE on, the click (2 .. 6 kHz, the first 15 ms) within 1 dB of it without, and the rumble alone 20 dB
 *     or more under the kick in those 15 ms (ducked under the attack: it neither covers nor softens it).
 *   cc -O2 -w -Ibuild/gen -Ifirmware/src -o build/host/kick_test tests/kick_test.c -lm */
#define KICK_MAIN kick_measure_main
#include "../tools/kick_measure.c"

static int hard(const char *n)
{
    static const char *const H[] = {"HARD", "SCHR", "GABB", "INDUS", "RAVE", "GROOV", "TRIBE", "WAREH", "DIRTY", "METAL",
                                    "PUNCH", "TOK", "ROLL", "DOUBL"};
    uint32_t i;
    for (i = 0; i < NELEM(H); i++)
        if (strstr(n, H[i]))
            return 1;
    return 0;
}
static float Y0[NK];
static double click_of(uint32_t e, uint32_t k, int rumble)
{
    pid_t p;
    keep_rumble = rumble;
    if (!(p = fork())) {
        one(e, k);
        _exit(0);
    }
    waitpid(p, 0, 0);
    measure("", "", Y, 0);
    return last_click;
}
/* the 2 .. 6 kHz band's energy in the first 15 ms (dB), and the whole energy there */
static double hf15(const float *x) { return 10 * log10(bandpass_energy(x, FS * 15u / 1000u, 2000, 6000) + 1); }
static double all15(const float *x)
{
    double e = 0;
    uint32_t i;
    for (i = 0; i < FS * 15u / 1000u; i++)
        e += (double)x[i] * x[i];
    return 10 * log10(e + 1);
}
int main(void)
{
    uint32_t e, k, n = 0, low = 0, classic = 0, masked = 0;
    double worst = 0, worst_m = 0;
    char what[200], wn[40] = "", wm[40] = "";
    int bad = 0;
    Y = mmap(0, NK * sizeof *Y, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0);
    for (e = 0; e < NENGINES; e++) {
        if (e != ENGI_DRUM && e != ENGI_DRUMX)
            continue;
        for (k = 0; k < ENGINES[e]->npresets; k++) {
            const preset_t *pr = &ENGINES[e]->presets[k];
            uint32_t m;
            int kp = 0;
            for (m = 0; m < NELEM(pr->more); m++)
                kp |= pr->more[m].id == P_KCLICK || pr->more[m].id == P_KPUNCH;
            if (strncmp(pr->name, "KT ", 3)) {
                classic += kp;
                continue;
            }
            {
                double c = click_of(e, k, 0), t = hard(pr->name) ? -30.0 : -32.0;
                n++;
                if (c < t) {
                    low++;
                    printf("kick:   %s %s: click %.1f dB (under %.0f)\n", ENGINES[e]->name, pr->name, c, t);
                }
                if (c - t < worst || !wn[0]) {
                    worst = c - t;
                    snprintf(wn, sizeof wn, "%s", pr->name);
                }
                {
                    int16_t mix = 0;
                    for (m = 0; m < NELEM(RB_KITS); m++)
                        if (RB_KITS[m][0] == k + (e == ENGI_DRUMX) * 128u)
                            mix = RB_KITS[m][2];
                    if (mix) {
                        static float d[NK];
                        double h0, h1, r15, k15;
                        uint32_t i;
                        memcpy(Y0, Y, sizeof Y0);   /* (Y: the kit without its RUMBLE, click_of above) */
                        h0 = hf15(Y0);
                        click_of(e, k, 1);
                        h1 = hf15(Y);
                        for (i = 0; i < NK; i++)
                            d[i] = Y[i] - Y0[i];
                        r15 = all15(d);
                        k15 = all15(Y0);
                        if (fabs(h1 - h0) > worst_m || k15 - r15 < 20) {
                            worst_m = fabs(h1 - h0) > worst_m ? fabs(h1 - h0) : worst_m;
                            snprintf(wm, sizeof wm, "%s: the rumble %.0f dB under", pr->name, k15 - r15);
                        }
                        masked += fabs(h1 - h0) > 1.0 || k15 - r15 < 20;
                    }
                }
            }
        }
    }
    snprintf(what, sizeof what, "kick: the %u techno kits' click over their bar (-30 hard, -32 techno): %u under; the closest "
             "%+.1f dB (%s)", n, low, worst, wn);
    printf("%-110s %s\n", what, low ? "FAIL" : "ok");
    bad += low != 0;
    snprintf(what, sizeof what, "kick: the classic kits leave K.CLICK and K.PUNCH at 0 (%u do not)", classic);
    printf("%-110s %s\n", what, classic ? "FAIL" : "ok");
    bad += classic != 0;
    snprintf(what, sizeof what, "kick: with its RUMBLE, the click moved %.1f dB at most (1), the rumble 20 dB under the kick in "
             "the first 15 ms (%s)", worst_m, wm);
    printf("%-110s %s\n", what, masked ? "FAIL" : "ok");
    bad += masked != 0;
    printf(bad ? "kick: %d FAILED\n" : "kick: all ok\n", bad);
    return bad != 0;
}
