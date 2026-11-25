/* SPDX-License-Identifier: GPL-3.0-only */
/* (Gir, rumble) the RUMBLE demos (livraison/wav_rumble): played by the firmware's own sequencer, engines, mix and FX
 * (tests/hostsim.c), each render in a process of its own (from power-on):
 *   boucle_150_sans.wav, boucle_150_avec.wav   4 bars at 150 BPM: KT HARD (kick on the beats, a clap, off-beat hats),
 *                                              RUMBLE 0 / the kit's
 *   boucle_150_sans_puis_avec.wav              the same, 4 bars without then 4 with (the rumble the only low: hard techno)
 *   boucle_150_basse_sans_puis_avec.wav        with the bass too, 4 bars without then 4 with
 *   kit_<NAME>.wav                             every kit with a RUMBLE: 2 bars of its kick, clap and hats at 150 BPM
 *                                              (SANS=1: each also with RUMBLE 0, kit_<NAME>_sans.wav)
 *   cc -O2 -w -Ibuild/gen -Ifirmware/src -o build/host/rumble_demo tools/rumble_demo.c -lm
 *   build/host/rumble_demo OUTDIR */
#define main hostsim_main
#include "../tests/hostsim.c"
#undef main
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define BPM 150
static uint32_t bar_frames(void) { return FS * 60u / BPM * 4u; }
static uint32_t find(uint32_t e, const char *name)
{
    uint32_t k;
    for (k = 0; k < ENGINES[e]->npresets; k++)
        if (!strcmp(ENGINES[e]->presets[k].name, name))
            return k;
    fprintf(stderr, "no %s\n", name);
    exit(1);
}
static void drums(track_t *t, uint32_t e, uint32_t pi)
{
    uint32_t i;
    host_preset(t, e, pi);
    t->p[P_AMODE] = 0;
    t->p[P_SLEN] = 16;
    t->p[P_SDIV] = 2;
    for (i = 0; i < 16u; i++) {
        uint8_t kk[3], n = 0;
        if (i % 4u == 0u) kk[n++] = 36;
        if (i % 8u == 4u) kk[n++] = 39;
        if (i % 4u == 2u) kk[n++] = 42;
        put_step(t, i, n, kk, n ? ST_NOTE : ST_REST, 0);
    }
}
/* bars of the loop into w; rumble_from: the bar from which the kit's RUMBLE plays (before it: RUMBLE 0); bass: with it */
static void play(FILE *w, uint32_t e, uint32_t pi, uint32_t bars, uint32_t rumble_from, int bass)
{
    uint32_t frames = bars * bar_frames() / CTL * CTL, f, i;
    int32_t o[2 * CTL], mix;
    host_tracks_init();
    song.g[G_BPM] = BPM;
    drums(&trk[0], e, pi);
    if (getenv("NODIST"))                              /* (the check: the track's DRIVE off) */
        trk[0].p[P_DIST] = 0;
    mix = trk[0].p[P_RB_MIX];
    if (bass) {
        static const uint8_t B[16] = {0, 0, 45, 0, 0, 0, 45, 0, 0, 0, 45, 0, 0, 0, 45, 45};
        track_t *t = &trk[1];
        host_preset(t, 0, find(0, "BS PSY ROLL"));
        t->p[P_AMODE] = 0;
        t->p[P_SLEN] = 16;
        t->p[P_SDIV] = 2;
        t->p[P_DKSRC] = 1;                               /* ducked by T1 */
        t->p[P_DKAMT] = 100;
        for (i = 0; i < 16u; i++)
            put_step(t, i, B[i] ? 1u : 0u, &B[i], B[i] ? ST_NOTE : ST_REST, 0);
    }
    transport_req = 1;
    for (f = 0; f < frames; f += CTL) {
        trk[0].p[P_RB_MIX] = (int16_t)(f / bar_frames() >= rumble_from ? mix : 0);
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++)
            wav_put(w, o[2 * i], o[2 * i + 1]);
    }
}
static void render(const char *dir, const char *name, uint32_t e, uint32_t pi, uint32_t bars, uint32_t from, int bass)
{
    char p[600];
    pid_t pid;
    int st;
    snprintf(p, sizeof p, "%s/%s.wav", dir, name);
    pid = fork();
    if (!pid) {
        FILE *w = fopen(p, "wb");
        if (!w) {
            perror(p);
            _exit(1);
        }
        wav_hdr(w, bars * bar_frames() / CTL * CTL);
        play(w, e, pi, bars, from, bass);
        fclose(w);
        _exit(0);
    }
    waitpid(pid, &st, 0);
    printf("%s\n", p);
}
int main(int argc, char **argv)
{
    uint32_t k;
    if (argc < 2) {
        fprintf(stderr, "usage: rumble_demo OUTDIR\n");
        return 2;
    }
    mkdir(argv[1], 0755);
    render(argv[1], "boucle_150_sans", ENGI_DRUM, find(ENGI_DRUM, "KT HARD"), 4, 99, 0);
    render(argv[1], "boucle_150_avec", ENGI_DRUM, find(ENGI_DRUM, "KT HARD"), 4, 0, 0);
    render(argv[1], "boucle_150_sans_puis_avec", ENGI_DRUM, find(ENGI_DRUM, "KT HARD"), 8, 4, 0);
    render(argv[1], "boucle_150_basse_sans_puis_avec", ENGI_DRUM, find(ENGI_DRUM, "KT HARD"), 8, 4, 1);
    for (k = 0; k < NELEM(RB_KITS); k++) {
        uint32_t e = RB_KITS[k][0] >= 128u ? ENGI_DRUMX : ENGI_DRUM, pi = RB_KITS[k][0] & 127u;
        char n[64], *c;
        snprintf(n, sizeof n, "kit_%s_%s", e == ENGI_DRUMX ? "DRUMX" : "DRUM", ENGINES[e]->presets[pi].name);
        for (c = n; *c; c++)
            if (*c == ' ')
                *c = '_';
        render(argv[1], n, e, pi, 2, 0, 0);
        if (getenv("SANS")) {                          /* (the same kit, RUMBLE 0: the rumble's level by difference) */
            strcat(n, "_sans");
            render(argv[1], n, e, pi, 2, 99, 0);
        }
    }
    return 0;
}
