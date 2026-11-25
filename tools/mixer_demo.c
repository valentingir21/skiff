/* SPDX-License-Identifier: GPL-3.0-only */
/* (techno-mixer) the isolator and DEPTH demos (livraison/wav_mixer): a techno loop at 145 BPM played by the firmware's
 * own sequencer, engines, mix and FX (tests/hostsim.c), the values turned at bar lines as a hand on the knobs would:
 *   kill_low.wav   8 bars: 2 bars whole, 2 bars with LOW in KILL on every track (the break), 2 bars back (the drop),
 *                  a bar with LOW KILL on the bass and the pad only (the kick alone in the low), a bar whole
 *   depth_pad.wav  8 bars: the pad at DEPTH 0 (in front) for 4 bars, then DEPTH 110 (at the back) for 4
 *   cc -O2 -w -Ibuild/gen -Ifirmware/src -o build/host/mixer_demo tools/mixer_demo.c -lm
 *   build/host/mixer_demo OUTDIR */
#define main hostsim_main
#include "../tests/hostsim.c"
#undef main
#include <sys/stat.h>

#define BPM 145
static uint32_t bar_frames(void) { return (uint32_t)(FS * 60.0 / BPM * 4); }
static uint32_t find(const char *eng, const char *name, uint32_t *pi)
{
    uint32_t e, k;
    for (e = 0; e < NENGINES; e++)
        if (eng_ok(e) && !strcmp(ENGINES[e]->name, eng))
            for (k = 0; k < ENGINES[e]->npresets; k++)
                if (!strcmp(ENGINES[e]->presets[k].name, name)) {
                    *pi = k;
                    return e;
                }
    fprintf(stderr, "no %s %s\n", eng, name);
    exit(1);
}
static void sound(track_t *t, const char *eng, const char *name)
{
    uint32_t pi, e = find(eng, name, &pi);
    host_preset(t, e, pi);
    t->p[P_AMODE] = 0;
    t->p[P_SLEN] = 16;
    t->p[P_SDIV] = 2;
}
/* the loop: T1 drums (DRUM-X INDUSTRIAL: kick on the beats, a clap, off-beat hats), T2 a rolling bass (MINI BS MOOG), T3 a
 * pad (SUPERSAW PA SS DARK, one chord a bar), T4 stabs (INDUS ST INDUS) */
static void loop_init(void)
{
    static const uint8_t BASS[16] = {0, 45, 45, 45, 0, 45, 45, 48, 0, 45, 45, 45, 0, 43, 45, 45};
    uint32_t i;
    host_tracks_init();
    song.g[G_BPM] = BPM;
    sound(&trk[0], "DRUM-X", "INDUSTRIAL");
    sound(&trk[1], "MINI", "BS MOOG");
    sound(&trk[2], "SUPERSAW", "PA SS DARK");
    sound(&trk[3], "INDUS", "ST INDUS");
    for (i = 0; i < 16u; i++) {
        uint8_t k[3] = {36, 39, 42}, n = 0, kk[3];
        if (i % 4u == 0u) kk[n++] = k[0];
        if (i % 8u == 4u) kk[n++] = k[1];
        if (i % 4u == 2u) kk[n++] = k[2];
        put_step(&trk[0], i, n, kk, n ? ST_NOTE : ST_REST, 0);
        put_step(&trk[1], i, BASS[i] ? 1u : 0u, &BASS[i], BASS[i] ? ST_NOTE : ST_REST, 0);
    }
    {
        static const uint8_t CH[3] = {57, 60, 64};
        put_step(&trk[2], 0, 3, CH, ST_NOTE, 0);
        for (i = 1; i < 16u; i++)
            put_step(&trk[2], i, 0, CH, ST_TIE, 0);
        put_step(&trk[3], 3, 1, &CH[0], ST_NOTE, 0);
        put_step(&trk[3], 11, 1, &CH[1], ST_NOTE, 0);
        for (i = 0; i < 16u; i++)
            if (i != 3u && i != 11u)
                put_step(&trk[3], i, 0, CH, ST_REST, 0);
    }
    trk[3].p[P_LEVEL] = 100;
    transport_req = 1;
}
static void render(const char *path, void (*at_bar)(uint32_t))
{
    uint32_t frames = 8u * bar_frames() / CTL * CTL, f, i, bar = 0xFFFFFFFFu;
    int32_t o[2 * CTL];
    FILE *w = fopen(path, "wb");
    if (!w) {
        perror(path);
        exit(1);
    }
    loop_init();
    wav_hdr(w, frames);
    for (f = 0; f < frames; f += CTL) {
        if (f / bar_frames() != bar) {
            bar = f / bar_frames();
            at_bar(bar);
        }
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++)
            wav_put(w, o[2 * i], o[2 * i + 1]);
    }
    fclose(w);
    printf("%s: 8 bars at %d BPM\n", path, BPM);
}
static void kill_low(uint32_t bar)
{
    int16_t all = bar == 2u || bar == 3u ? 0 : 64, some = bar == 6u ? 0 : all;
    trk[0].p[P_EQ_LOW] = all;                           /* bars 3-4: LOW KILL on every track (the break), back on bar 5 */
    trk[1].p[P_EQ_LOW] = some;                          /* (the drop); bar 7: the bass and the pad only, the kick on */
    trk[2].p[P_EQ_LOW] = some;
    trk[3].p[P_EQ_LOW] = all;
}
static void depth_pad(uint32_t bar) { trk[2].p[P_DEPTH] = (int16_t)(bar < 4u ? 0 : 110); }
int main(int argc, char **argv)
{
    char p[512];
    if (argc < 2) {
        fprintf(stderr, "usage: mixer_demo OUTDIR\n");
        return 2;
    }
    mkdir(argv[1], 0755);
    snprintf(p, sizeof p, "%s/kill_low.wav", argv[1]);
    render(p, kill_low);
    snprintf(p, sizeof p, "%s/depth_pad.wav", argv[1]);
    render(p, depth_pad);
    return 0;
}
