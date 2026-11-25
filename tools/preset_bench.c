/* SPDX-License-Identifier: GPL-3.0-only */
/* (techno-presets) Listening bench: every factory preset whose name starts with one of the given prefixes, rendered
 * through the firmware's own engines, voices and FX (tests/hostsim.c, as tests/regress.c), as the preset loads on the
 * unit (host_preset: ui.c preset_fill's values), into OUTDIR/<ENGINE>_<NN>_<NAME>.wav: a short phrase at 145 BPM that
 * suits its kind (the name's prefix): LD a lead line (legato overlaps; POLY ones chords), BS a bass line (ACID: a
 * 303 line with accents and slides), PA pads, ST off-beat stabs, DR a held drone, FX single hits, KT a beat.
 *   cc -O2 -w -Ibuild/gen -Ifirmware/src -o build/host/preset_bench tools/preset_bench.c -lm
 *   build/host/preset_bench OUTDIR [PREFIX|NAME ..]     (default prefixes: LD BS PA ST DR FX KT; a whole name: that one)
 * Prints each render's peak and RMS (16-bit units) and whether every voice was free at the end. */
#define main hostsim_main
#include "../tests/hostsim.c"
#undef main
#include <sys/stat.h>

#define BPM 145.0
static double step_s(void) { return 60.0 / BPM / 4.0; }   /* a 1/16 */

typedef struct { double t; uint8_t on, note, vel; } ev_t;
static ev_t ev[1024];
static uint32_t nev;
static void note(double step, double len, uint32_t n, uint32_t vel)
{
    if (nev + 2u > NELEM(ev))
        return;
    ev[nev++] = (ev_t){step * step_s(), 1, (uint8_t)n, (uint8_t)vel};
    ev[nev++] = (ev_t){(step + len) * step_s(), 0, (uint8_t)n, 0};
}
static int ev_cmp(const void *a, const void *b)
{
    const ev_t *x = a, *y = b;
    if (x->t != y->t)
        return x->t < y->t ? -1 : 1;
    return (int)x->on - (int)y->on;               /* at the same time: the note-off first (a retrigger) */
}
static void chord(double step, double len, const uint8_t *n, uint32_t k, uint32_t vel)
{
    uint32_t i;
    for (i = 0; i < k; i++)
        note(step, len, n[i], vel);
}

/* the phrases, A minor; 2 bars = 32 steps unless said */
static double phrase_lead(int poly)
{
    static const uint8_t M[16] = {69, 72, 76, 74, 72, 76, 79, 76, 69, 72, 76, 77, 76, 74, 72, 71};
    static const uint8_t C[4][3] = {{57, 60, 64}, {53, 57, 60}, {55, 59, 62}, {52, 55, 59}};
    uint32_t i;
    if (poly) {
        for (i = 0; i < 4u; i++)
            chord(i * 8.0, 7.5, C[i], 3, 100);
        return 32;
    }
    for (i = 0; i < 16u; i++)                     /* 1/8 notes, every 4th tied over into the next: a slide */
        note(i * 2.0, i % 4u == 3u ? 2.3 : 1.6, M[i], i % 4u == 0u ? 120 : 96);
    return 32;
}
static double phrase_pluck(void)
{
    static const uint8_t A[8] = {69, 76, 72, 79, 69, 76, 72, 81};
    uint32_t i;
    for (i = 0; i < 32u; i++)
        note(i, 0.5, A[i % 8u] - (i >= 16u ? 4u : 0u), i % 4u == 0u ? 120 : 90);
    return 32;
}
/* the 303 line: note (0 rest), accent, slide into the next */
static const struct { uint8_t n, acc, sl; } ACID_LINE[16] = {
    {33, 1, 0}, {33, 0, 0}, {45, 0, 1}, {33, 0, 0}, {36, 1, 0}, {0, 0, 0}, {43, 0, 1}, {45, 1, 0},
    {33, 0, 0}, {40, 0, 1}, {33, 1, 0}, {45, 0, 0}, {31, 0, 1}, {33, 1, 1}, {45, 0, 0}, {48, 1, 0}};
static double phrase_acid(void)                   /* (other engines' ACID basses: the line as notes, accents loud) */
{
    uint32_t r, i;
    for (r = 0; r < 2u; r++)
        for (i = 0; i < 16u; i++)
            if (ACID_LINE[i].n)
                note(r * 16.0 + i, ACID_LINE[i].sl ? 1.25 : 0.55, ACID_LINE[i].n, ACID_LINE[i].acc ? 127 : 80);
    return 32;
}
/* ((techno-engines) ACID: the line as the sequencer's steps, ACC and SLD, GATE 50 %: the 303's accent and slide come
 * from the steps, as on the unit. Two passes, then the transport stops) */
static int seq_line;
static void acid_steps(track_t *t)
{
    uint32_t i;
    for (i = 0; i < 16u; i++) {
        uint8_t nt = ACID_LINE[i].n;
        put_step(t, i, nt ? 1u : 0u, &nt, nt ? ST_NOTE : ST_REST,
                 (ACID_LINE[i].acc ? SF_ACCENT : 0u) | (ACID_LINE[i].sl ? SF_SLIDE : 0u));
    }
    t->p[P_SLEN] = 16;
    t->p[P_SDIV] = 2;                             /* 1/16 */
    t->p[P_SGATE] = 64;
    transport_req = 1;
}
static double phrase_bass(int rumble)
{
    uint32_t i;
    if (rumble) {                                 /* a long note a beat, under where the kick would be */
        for (i = 0; i < 8u; i++)
            note(i * 4.0, 3.2, i < 4u ? 33 : 29, 110);
        return 32;
    }
    for (i = 0; i < 32u; i++)                     /* a rolling bass: the three 1/16s after each beat */
        if (i % 4u)
            note(i, 0.7, (i >= 16u ? 29 : 33) + (i % 4u == 3u ? 12 : 0), i % 4u == 2u ? 110 : 90);
    return 32;
}
static double phrase_pad(void)
{
    static const uint8_t C[2][4] = {{45, 57, 60, 64}, {41, 53, 57, 60}};
    chord(0, 15.5, C[0], 4, 100);
    chord(16, 15.5, C[1], 4, 100);
    return 32;
}
static double phrase_stab(int mono)
{
    static const uint8_t C[2][4] = {{57, 60, 64, 67}, {53, 57, 60, 64}};
    uint32_t i;
    for (i = 0; i < 16u; i++)                     /* the off-beat 1/8s */
        if (mono)
            note(i * 2.0 + 1.0, 0.8, i < 8u ? 57 : 53, 110);
        else
            chord(i * 2.0 + 1.0, 0.8, C[i / 8u], 4, 110);
    return 32;
}
static double phrase_drone(void)
{
    note(0, 30, 45, 100);
    return 32;
}
static double phrase_fx(int riser)
{
    if (riser) {
        note(0, 28, 57, 110);
        return 32;
    }
    note(0, 6, 45, 120);
    note(16, 6, 57, 120);
    return 32;
}
static double phrase_kit(void)
{
    uint32_t i;
    for (i = 0; i < 32u; i++) {
        if (i % 4u == 0u)
            note(i, 0.5, 36, 120);                /* kick */
        if (i % 4u == 2u)
            note(i, 0.5, 46, 100);                /* open hat */
        else
            note(i, 0.3, 42, i % 2u ? 70 : 90);   /* closed hat */
        if (i % 8u == 4u)
            note(i, 0.5, 39, 115);                /* clap */
    }
    return 32;
}

static double make_phrase(uint32_t e, const preset_t *p)
{
    const char *n = p->name;
    nev = 0;
#ifdef ENGI_ACID
    if (e == ENGI_ACID) {                         /* ((techno-engines) the 303: its line on the sequencer) */
        seq_line = 1;
        return 32;
    }
#endif
    (void)e;
    if (!strncmp(n, "BS", 2))
        return strstr(n, "ACID") ? phrase_acid() : strstr(n, "RUMBLE") || strstr(n, "SUB") ? phrase_bass(1)
                                                                                             : phrase_bass(0);
    if (!strncmp(n, "LD", 2))
        return p->tag == PT_PLUCK ? phrase_pluck() : phrase_lead(!p->mono);
    if (!strncmp(n, "PA", 2))
        return phrase_pad();
    if (!strncmp(n, "ST", 2))
        return phrase_stab(p->mono);
    if (!strncmp(n, "DR", 2))
        return phrase_drone();
    if (!strncmp(n, "KT", 2))
        return phrase_kit();
    if (p->tag == PT_BASS)
        return strstr(n, "ACID") ? phrase_acid() : phrase_bass(0);
    if (p->tag == PT_LEAD)
        return phrase_lead(!p->mono);
    if (p->tag == PT_PAD)
        return phrase_pad();
    return phrase_fx(strstr(n, "RISER") != 0);
}

static int voices_free(void)
{
    uint32_t p, i;
    for (p = 0; p < NPART; p++)
        for (i = 0; i < NVOICE; i++)
            if (trk[p].v[i].active)
                return 0;
    return 1;
}

static void render(const char *path, uint32_t e, uint32_t pi)
{
    const preset_t *p = &ENGINES[e]->presets[pi];
    double len = make_phrase(e, p) * step_s() + 2.5;   /* the phrase, then its tail */
    uint32_t frames = (uint32_t)(len * FS) / CTL * CTL, f, k = 0, i;
    int32_t o[2 * CTL];
    int32_t peak = 0;
    double ss = 0;
    track_t *t = &trk[0];
    FILE *w = fopen(path, "wb");
    if (!w) {
        perror(path);
        exit(1);
    }
    qsort(ev, nev, sizeof ev[0], ev_cmp);
    host_tracks_init();
    song.g[G_BPM] = (int16_t)BPM;
    host_preset(t, e, pi);
    t->p[P_AMODE] = 0;                            /* (no arp: the phrase plays the notes) */
    if (seq_line)
        acid_steps(t);
    wav_hdr(w, frames);
    for (f = 0; f < frames; f += CTL) {
        double now = (double)f / FS;
        while (k < nev && ev[k].t <= now) {
            if (ev[k].on)
                input_on(t, ev[k].note, ev[k].vel);
            else
                input_off(t, ev[k].note);
            k++;
        }
        if (seq_line && now >= 32 * step_s() && song.playing)
            transport_req = 2;
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++) {
            int32_t l = o[2 * i], r = o[2 * i + 1], a = l < 0 ? -l : l, b = r < 0 ? -r : r;
            peak = a > peak ? a : b > peak ? b : peak;
            ss += (double)l * l + (double)r * r;
            wav_put(w, l, r);
        }
    }
    fclose(w);
    seq_line = 0;
    nev = 0;
    printf("%-8s %2u %-13s peak %5d  rms %5.0f  %s\n", ENGINES[e]->name, pi, p->name, peak,
           sqrt(ss / (2.0 * frames)), voices_free() ? "free" : "STILL SOUNDING");
}

int main(int argc, char **argv)
{
    static const char *const DEF[] = {"LD", "BS", "PA", "ST", "DR", "FX", "KT"};
    const char *const *pre = argc > 2 ? (const char *const *)argv + 2 : DEF;
    uint32_t npre = argc > 2 ? (uint32_t)argc - 2u : NELEM(DEF), e, pi, i;
    char path[512], slug[32];
    if (argc < 2) {
        fprintf(stderr, "usage: preset_bench OUTDIR [PREFIX ..]\n");
        return 2;
    }
    mkdir(argv[1], 0755);
    for (e = 0; e < NENGINES; e++)
        for (pi = 0; eng_ok(e) && pi < ENGINES[e]->npresets; pi++) {
            const char *n = ENGINES[e]->presets[pi].name;
            for (i = 0; i < npre && strcmp(n, pre[i]) && !(!strncmp(n, pre[i], strlen(pre[i])) && n[strlen(pre[i])] == ' '); i++)
                ;
            if (i == npre)
                continue;
            for (i = 0; n[i] && i + 1 < sizeof slug; i++)
                slug[i] = n[i] == ' ' ? '_' : n[i];
            slug[i] = 0;
            snprintf(path, sizeof path, "%s/%s_%02u_%s.wav", argv[1], ENGINES[e]->name, pi, slug);
            render(path, e, pi);
        }
    return 0;
}
