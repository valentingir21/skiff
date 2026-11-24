/* SPDX-License-Identifier: GPL-3.0-only */
/* (techno-engines) ACID: a TB-303 voice. Written for Skiff; its filter is skiff-atlatl's ACID (LIkst4, GPL-3.0-only:
 * firmware/src/flt.c fl_acid), itself Open303's TeeBeeFilter (Robin Schmidt, MIT) in its TB_303 mode as X0X's
 * bass303.c runs it (charlesvestal/fm1-x0x, GPL-3.0-only): four one-poles that load each other (the diode ladder's
 * 18 dB/oct-ish slope), the resonance fed back through a high-pass (the bass stays when it rings: CHAR moves it, 63 ~
 * the 303's 150 Hz), a 44 Hz high-pass before it, the input soft-clipped (it squelches instead of running away), b0 and
 * k glided across the block. Its tables (ACID_B0 ACID_K ACID_R) are computed as atlatl's tools/gen_flt_tables.py does.
 *   the oscillator  WAVE SAW or SQR (dsp.c: polyBLEP), the 303's two
 *   the filter env  its own (not the track's ADSR): every struck note restarts it at the top, it falls with DECAY
 *                   (30 ms .. 2.5 s, exponential); ENV MOD how far it opens the filter (up to ~6 octaves)
 *   ACCENT          a note at velocity 118 or more is accented: a sequencer step's ACC plays at 127, a normal one at 96
 *                   (the sequencer is untouched: the 303's accent from what Skiff sends). An accented note's filter
 *                   envelope falls at the shortest DECAY (the 303's) and lifts the level by ACCENT with it (the 303's
 *                   accented VCA); it also charges the accent capacitor (~60 ms up, ~250 ms down) that opens the filter
 *                   by ACCENT: one accent half charges it, accents close together pile up (the 303's "wow": the 4th of
 *                   four 1/16 accents ~1.8 times the first's sweep)
 *   SLIDE           the track's GLIDE in GL MODE TIME (the presets: ~60 ms whatever the interval) on LEG: a SLD step or
 *                   an overlapping key glides there and does not restart the envelope (voice.c mono_play: no note_on)
 *   DRIVE           a soft clip after the filter (hardtech acid)
 * One voice (engine_t.poly: a 303 is monophonic; POLY plays it as one voice too). State in the part's engine area
 * (acid_vc_t: the filter's ten states, the envelope, the accent). */
#define ENGI_ACID 16u            /* engines.c ENGINES[] (append-only) */
static const char *const N_ACID_WAVE[] = {"SAW", "SQR"};

typedef struct {
    int32_t y[4], fx, fy, li, lo;   /* the ladder's stages, the feedback high-pass, the input's and the output's high-pass */
    int32_t bq, kq;                 /* b0 and k a block ago (0: none yet) */
    int32_t env;                    /* the filter envelope, Q24 */
    int32_t cap;                    /* the accent capacitor, Q24 */
    int32_t acc;                    /* this note accented */
} acid_vc_t;

/* computed as atlatl's tools/gen_flt_tables.py (Open303's TeeBeeFilter at 44.1 kHz, every 8th cutoff index 0..1024,
 * 20 Hz .. ~17.8 kHz over 9.8 octaves, held below 12 kHz): b0 Q16, k Q8 at RESO 1; RESO 0..127 -> Open303's skew,
 * Q12; DECAY 0..127 -> the envelope's factor a block (32 samples), Q16, tau 30 ms .. 2.5 s */
static const uint16_t ACID_B0[129] = {
    159, 166, 174, 182, 190, 198, 208, 217, 227, 238, 249, 261, 274, 287, 301, 316,
    331, 347, 364, 382, 401, 421, 442, 465, 488, 513, 539, 566, 595, 625, 657, 690,
    725, 762, 801, 842, 885, 931, 978, 1029, 1081, 1137, 1195, 1256, 1320, 1387, 1458, 1532,
    1610, 1692, 1778, 1868, 1962, 2061, 2164, 2273, 2386, 2505, 2629, 2759, 2894, 3036, 3184, 3339,
    3500, 3668, 3843, 4025, 4215, 4412, 4617, 4831, 5052, 5281, 5519, 5765, 6020, 6283, 6555, 6836,
    7126, 7424, 7731, 8046, 8370, 8702, 9042, 9389, 9745, 10108, 10477, 10854, 11236, 11624, 12018, 12416,
    12818, 13224, 13633, 14044, 14457, 14871, 15285, 15698, 16110, 16520, 16927, 17331, 17730, 18124, 18512, 18893,
    19267, 19633, 19989, 20337, 20674, 21000, 21314, 21617, 21906, 22057, 22057, 22057, 22057, 22057, 22057, 22057,
    22057,
};
static const uint16_t ACID_K[129] = {
    4369, 4370, 4371, 4372, 4373, 4375, 4376, 4377, 4379, 4380, 4382, 4383, 4385, 4387, 4389, 4391,
    4393, 4395, 4397, 4400, 4403, 4405, 4408, 4411, 4415, 4418, 4422, 4426, 4430, 4434, 4438, 4443,
    4448, 4453, 4459, 4465, 4471, 4478, 4485, 4492, 4499, 4508, 4516, 4525, 4535, 4545, 4555, 4567,
    4579, 4591, 4604, 4618, 4633, 4649, 4665, 4682, 4701, 4720, 4741, 4762, 4785, 4809, 4835, 4862,
    4890, 4920, 4952, 4986, 5022, 5059, 5099, 5142, 5186, 5234, 5284, 5337, 5393, 5453, 5516, 5583,
    5654, 5730, 5809, 5894, 5984, 6079, 6181, 6288, 6403, 6524, 6653, 6790, 6936, 7092, 7257, 7432,
    7619, 7818, 8030, 8255, 8495, 8751, 9023, 9313, 9621, 9949, 10298, 10669, 11063, 11482, 11926, 12397,
    12895, 13421, 13976, 14560, 15173, 15814, 16482, 17175, 17890, 18283, 18283, 18283, 18283, 18283, 18283, 18283,
    18283,
};
static const uint16_t ACID_R[128] = {
    0, 101, 199, 295, 389, 480, 570, 657, 742, 826, 907, 986, 1064, 1140, 1214, 1286,
    1357, 1426, 1493, 1559, 1623, 1686, 1747, 1807, 1865, 1922, 1978, 2033, 2086, 2138, 2188, 2238,
    2286, 2334, 2380, 2425, 2469, 2512, 2554, 2595, 2635, 2674, 2712, 2750, 2786, 2822, 2856, 2890,
    2924, 2956, 2988, 3018, 3049, 3078, 3107, 3135, 3162, 3189, 3215, 3241, 3266, 3290, 3314, 3337,
    3360, 3382, 3404, 3425, 3446, 3466, 3486, 3505, 3524, 3542, 3560, 3578, 3595, 3611, 3628, 3644,
    3659, 3674, 3689, 3704, 3718, 3732, 3745, 3759, 3771, 3784, 3796, 3808, 3820, 3831, 3843, 3854,
    3864, 3875, 3885, 3895, 3905, 3914, 3923, 3932, 3941, 3950, 3958, 3966, 3974, 3982, 3990, 3997,
    4005, 4012, 4019, 4026, 4032, 4039, 4045, 4051, 4057, 4063, 4069, 4075, 4080, 4086, 4091, 4096,
};
static const uint16_t ACID_DCY[128] = {
    63970, 64023, 64074, 64124, 64171, 64218, 64262, 64305, 64347, 64388, 64427, 64464, 64501, 64536, 64570, 64603,
    64634, 64665, 64695, 64723, 64751, 64778, 64803, 64828, 64852, 64876, 64898, 64920, 64941, 64961, 64981, 65000,
    65018, 65036, 65053, 65069, 65085, 65100, 65115, 65130, 65144, 65157, 65170, 65182, 65194, 65206, 65217, 65228,
    65239, 65249, 65259, 65268, 65277, 65286, 65295, 65303, 65311, 65319, 65326, 65333, 65340, 65347, 65353, 65360,
    65366, 65371, 65377, 65382, 65388, 65393, 65398, 65402, 65407, 65411, 65416, 65420, 65424, 65428, 65431, 65435,
    65438, 65442, 65445, 65448, 65451, 65454, 65457, 65459, 65462, 65465, 65467, 65469, 65472, 65474, 65476, 65478,
    65480, 65482, 65484, 65486, 65487, 65489, 65491, 65492, 65494, 65495, 65496, 65498, 65499, 65500, 65502, 65503,
    65504, 65505, 65506, 65507, 65508, 65509, 65510, 65511, 65512, 65513, 65513, 65514, 65515, 65516, 65516, 65517,
};
#define ACID_ACC_DCY 54          /* an accented note's DECAY (tau ~200 ms: the 303's shortest) */

static acid_vc_t *acid_vc(const track_t *t, const voice_t *v)
{
    return &((acid_vc_t *)eng_part(t))[(uint32_t)(v - t->v) % NVOICE];
}

static void acid_note_on(track_t *t, voice_t *v)
{
    acid_vc_t *A = acid_vc(t, v);
    if (!v->env_out) {                                /* from silence: a clean filter */
        int32_t cap = A->cap;
        memset(A, 0, sizeof *A);
        A->cap = cap;                                 /* (the accent's charge lives on between notes, as the 303's) */
    }
    A->env = 1 << 24;                                 /* a struck note: the envelope from the top (a slide: no note_on) */
    A->acc = v->vel >= 118u;
}

/* Open303's filter on n samples of b (the level x 4, as atlatl's flt.c), cutoff index ci 0..1024, RESO 0..127, CHAR
 * 0..127: atlatl's fl_acid without its BLEND */
static void acid_filter(acid_vc_t *A, int32_t *b, uint32_t n, int32_t ci, int32_t res, int32_t chr)
{
    int32_t j = clamp(ci + 104, 0, 1024), i8 = j >> 3, fr = j & 7, i9 = i8 < 128 ? i8 + 1 : 128;
    int32_t b0 = ACID_B0[i8] + (((ACID_B0[i9] - ACID_B0[i8]) * fr) >> 3);          /* Q16 */
    int32_t k1 = ACID_K[i8] + (((ACID_K[i9] - ACID_K[i8]) * fr) >> 3);             /* Q8, at RESO 1 */
    int32_t r = ACID_R[clamp(res, 0, 127)], kr = (k1 * r) >> 12;                    /* Q12; Q8 */
    int32_t g2 = (((((k1 * 16) / 17 - 4096) * r) >> 12) + 4096) * ((4096 + r) >> 1) >> 10;   /* Q12 */
    int32_t hz = 30 + chr * chr / 40, fa = 65536 - ((hz * 598) >> 6), fb0 = (65536 + fa) >> 1;   /* FB HP, Q16 */
    int32_t y1 = A->y[0], y2 = A->y[1], y3 = A->y[2], y4 = A->y[3], fx = A->fx, fy = A->fy, li = A->li, lo = A->lo;
    int32_t bq = A->bq ? A->bq : b0, kq = A->bq ? A->kq : kr;
    int32_t db = (b0 - bq) / (int32_t)n, dk = (kr - kq) / (int32_t)n;
    uint32_t i;
    for (i = 0; i < n; i++) {
        int32_t x = clamp(b[i] >> 2, -32000, 32000) << 6, tt, u, o, hy;
        bq += db;
        kq += dk;
        hy = x - li;                                                    /* 44.5 Hz high-pass */
        li += (int32_t)(((int64_t)hy * 415) >> 16);
        tt = (int32_t)(((int64_t)kq * y4) >> 8);                        /* the resonance, through its high-pass */
        fy = (int32_t)(((int64_t)(tt - fx) * fb0 + (int64_t)fy * fa) >> 16);
        fx = tt;
        u = clamp(hy - fy, -60000000, 60000000);
        u = (softclip(u >> 6) << 6) + (u & 63);                        /* the squelch */
        y1 += (int32_t)(((int64_t)(bq << 1) * (u - y1 + y2)) >> 16);
        y2 += (int32_t)(((int64_t)bq * (y1 - 2 * y2 + y3)) >> 16);
        y3 += (int32_t)(((int64_t)bq * (y2 - 2 * y3 + y4)) >> 16);
        y4 = clamp(y4 + (int32_t)(((int64_t)bq * (y3 - 2 * y4)) >> 16), -25000000, 25000000);
        o = (int32_t)(((((int64_t)g2 * y4) >> 18) * (4096 + r)) >> 12);
        o = clamp(o, -2000000, 2000000) << 8;
        lo += (int32_t)(((int64_t)(o - lo) * 224) >> 16);               /* 24 Hz high-pass */
        b[i] = clamp((o - lo) >> 8, -65000, 65000) << 2;
    }
    A->y[0] = y1, A->y[1] = y2, A->y[2] = y3, A->y[3] = y4, A->fx = fx, A->fy = fy, A->li = li, A->lo = lo;
    A->bq = b0;
    A->kq = kr;
}

static void acid_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    acid_vc_t *A = acid_vc(t, v);
    int32_t buf[CTL], e15, c15, ci, gain, drv = p[P_E6];
    uint32_t i, ph = v->ph[0], inc = m->inc, sq = p[P_E0] != 0;
    if (n > CTL)
        n = CTL;
    A->env = (int32_t)(((int64_t)A->env * ACID_DCY[A->acc ? (p[P_E4] < ACID_ACC_DCY ? p[P_E4] : ACID_ACC_DCY)
                                                          : clamp(p[P_E4], 0, 127)]) >> 16);
    {   /* the accent capacitor: charged towards the envelope while the note is accented and held, then drains */
        int32_t tg = A->acc && v->gate ? A->env : 0;
        A->cap += (int32_t)(((int64_t)(tg - A->cap) * (tg > A->cap ? 600 : 190)) >> 16);   /* (~60 ms up, ~250 ms down) */
    }
    e15 = A->env >> 9;
    c15 = A->cap >> 9;
    ci = p[P_E1] * 6 + (((e15 >> 7) * p[P_E3] * 5) >> 8) + (((c15 >> 7) * p[P_E5] * 5) >> 8) + (m->cutoff >> 5);
    gain = 4096 + (A->acc ? (e15 * p[P_E5]) >> 11 : 0);                    /* Q12: the accent's level, the note's own */
    for (i = 0; i < n; i++) {
        buf[i] = (sq ? osc_pulse(ph, inc, 0x80000000u) : osc_saw(ph, inc)) << 2;
        ph += inc;
    }
    acid_filter(A, buf, n, ci, p[P_E2], p[P_E7]);
    for (i = 0; i < n; i++) {
        int32_t s = (buf[i] >> 2) * gain >> 12;
        if (drv)
            s = softclip((s * (16 + drv)) >> 4);
        else
            s = soft_knee(s, 24000);
        out[i] += voice_amp2((s * 5) >> 3, m, i);                      /* (x 1.25: level with ANALOG's basses) */
    }
    v->ph[0] = ph;
}

static const preset_t ACID_PRESETS[] = {
    /* name, {WAVE, CUTOFF, RES, ENV MOD, DECAY, ACCENT, DRIVE, CHAR}, {A D S R}, fenv, mono, sends, more, type */
    {"BS 303", {0, 30, 95, 80, 60, 80, 0, 63}, {0, 100, 127, 12}, 0, 1, FX(0, 0, 30, 10), MORE({P_GLMODE, 1}, {P_GLIDE, 26}), TAG(PT_BASS)},
    {"BS 303 RESO", {0, 24, 118, 70, 66, 90, 0, 70}, {0, 100, 127, 12}, 0, 1, FX(0, 0, 40, 15), MORE({P_GLMODE, 1}, {P_GLIDE, 26}), TAG(PT_BASS)},
    {"BS 303 HARD", {0, 36, 105, 85, 52, 100, 90, 55}, {0, 100, 127, 10}, 0, 1, FX(40, 0, 25, 8), MORE({P_GLMODE, 1}, {P_GLIDE, 26}, {P_DTYPE, 1}), TAG(PT_BASS)},
    {"BS 303 DARK", {1, 14, 90, 55, 70, 60, 10, 80}, {0, 100, 127, 14}, 0, 1, FX(0, 0, 20, 20), MORE({P_GLMODE, 1}, {P_GLIDE, 26}), TAG(PT_BASS)},
    {"BS 303 SQLCH", {0, 20, 124, 100, 40, 110, 20, 63}, {0, 100, 127, 10}, 0, 1, FX(10, 0, 30, 10), MORE({P_GLMODE, 1}, {P_GLIDE, 26}), TAG(PT_BASS)},
    {"BS PHUTURE", {0, 26, 112, 90, 74, 95, 15, 50}, {0, 100, 127, 14}, 0, 1, FX(10, 0, 35, 20), MORE({P_GLMODE, 1}, {P_GLIDE, 26}), TAG(PT_BASS)},
    {"BS 303 DIST", {1, 34, 110, 90, 56, 110, 120, 60}, {0, 100, 127, 10}, 0, 1, FX(70, 0, 15, 8), MORE({P_GLMODE, 1}, {P_GLIDE, 26}, {P_DTYPE, 4}), TAG(PT_BASS)},
    {"LD 303 TRNC", {0, 40, 100, 75, 80, 70, 0, 63}, {0, 100, 127, 20}, 0, 1, FX(0, 30, 60, 40), MORE({P_GLMODE, 1}, {P_GLIDE, 26}), TAG(PT_LEAD)},
};

static const engine_t ENG_ACID = {
    .name = "ACID",
    .page_title = {"303", "ACCENT"},
    .edit = {
        {"WAVE", N_ACID_WAVE, 0, 1, 0, F_ENUM, 0},
        {"CUTOFF", 0, 0, 127, 40, F_CUTOFF, 0},
        {"RES", 0, 0, 127, 90, F_PCT, 0},
        {"ENV MOD", 0, 0, 127, 70, F_PCT, 0},
        {"DECAY", 0, 0, 127, 60, F_PCT, 0},
        {"ACCENT", 0, 0, 127, 80, F_PCT, 0},
        {"DRIVE", 0, 0, 127, 0, F_PCT, 0},
        {"CHAR", 0, 0, 127, 63, F_PCT, 0},
    },
    .presets = ACID_PRESETS,
    .npresets = NELEM(ACID_PRESETS),
    .note_on = acid_note_on,
    .render = acid_render,
    .knob = {P_E1, P_E2, P_E3, P_E4},
    .poly = 1,
};
