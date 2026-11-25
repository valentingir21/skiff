/* SPDX-License-Identifier: GPL-3.0-or-later
 * (techno-engines) ACID's pattern generator and mutator: the ATEK303 SEQ's (animatek/FM1-Animatek,
 * atek303/app/dsp/acid_pattern.c, GPL-3.0-or-later, Copyright (C) 2026 Javier Melgar (Animatek); itself a port of its
 * VCV Rack module's src/AcidPattern.hpp and AcidGen.hpp, "tuned on a corpus of acid lines"), in integers: ATEK303 draws
 * its numbers in float; here every draw is the same 24 bits of the same xorshift stream, compared with the same
 * thresholds in Q24 (tests/acid_pattern_test.c compares the two over 20000 seeds: the patterns are ATEK's, a rare
 * one off by a rounding at a threshold). The character knobs are the ATEK303 SEQ's defaults (DENSITY 0.65, ACCENT 0.6,
 * SLIDE 0.5, RANGE 0.45, TIE 0.4), so their curves are worked out here once.
 * The pattern in two layers, as ATEK's: time (REST, NOTE or TIE per step) and pitch (one event per NOTE: scale degree,
 * octave, accent, slide out). ui_tools.c turns it into the track's steps (acid_to_steps) and back (acid_from_steps). */
#define ACID_MAX_STEPS 64
enum { ACID_REST = 0, ACID_NOTE = 1, ACID_TIE = 2 };
enum { ACID_MUT_TIME = 1, ACID_MUT_PITCH = 2, ACID_MUT_ARTICULATION = 3 };
typedef struct { int8_t degree, octave; uint8_t accent, slide_out; } acid_pitch_t;
typedef struct {
    uint32_t seed;
    uint8_t time_length, pitch_length;
    uint8_t time[ACID_MAX_STEPS];
    acid_pitch_t pitch[ACID_MAX_STEPS];
} acid_pattern_t;
typedef struct { uint8_t n; int8_t s[12]; } acid_scale_t;
#define ACID_NSCALE 8
static const acid_scale_t ACID_SCALES[ACID_NSCALE] = {   /* ACID (minor pentatonic) MIN PHRY HARM DOR BLUES MAJ CHR */
    {5, {0, 3, 5, 7, 10}}, {7, {0, 2, 3, 5, 7, 8, 10}}, {7, {0, 1, 3, 5, 7, 8, 10}}, {7, {0, 2, 3, 5, 7, 8, 11}},
    {7, {0, 2, 3, 5, 7, 9, 10}}, {6, {0, 3, 5, 6, 7, 10}}, {7, {0, 2, 4, 5, 7, 9, 11}},
    {12, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}}};
#define ACID_CHR 7
#define Q24(x) ((int32_t)((x) * 16777216.0 + 0.999999))   /* (ceil: u < x in float is u24 < Q24(x)) */
enum { C_PEDAL, C_RISE, C_FALL, C_ZIGZAG, C_SCATTER, CONTOUR_LEN };

static int ap_min(int a, int b) { return a < b ? a : b; }
static int ap_max(int a, int b) { return a > b ? a : b; }
static int ap_abs(int x) { return x < 0 ? -x : x; }

static int acid_semi_of(int d, int o, int scale)
{
    const acid_scale_t *sc = &ACID_SCALES[scale];
    int i = (d % sc->n + sc->n) % sc->n;
    return sc->s[i] + 12 * (o + (d - i) / sc->n);
}

/* AcidCorpus.hpp's aggregate frequencies, x 100 (pitch classes) and Q24 (the rest) */
static const uint16_t PITCH_CLASS_WEIGHT[12] = {538, 138, 123, 173, 112, 181, 100, 146, 173, 119, 219, 108};
#define VOCAB_MAX 7
static const int32_t VOCAB_SIZE_CDF[VOCAB_MAX] = {Q24(0.143f), Q24(0.264f), Q24(0.457f), Q24(0.636f), Q24(0.772f),
                                                  Q24(0.851f), Q24(1.0f)};
static const int32_t SLIDE_BY_LEAP[13] = {Q24(0.074f), Q24(0.274f), Q24(0.348f), Q24(0.217f), Q24(0.525f), Q24(0.307f),
                                          Q24(0.406f), Q24(0.241f), Q24(0.307f), Q24(0.226f), Q24(0.280f), Q24(0.320f),
                                          Q24(0.391f)};
static int32_t semi_weight(int semi) { return PITCH_CLASS_WEIGHT[((semi % 12) + 12) % 12]; }

static void pitch_zero(acid_pitch_t *p) { p->degree = p->octave = 0; p->accent = p->slide_out = 0; }
static void acid_clear(acid_pattern_t *pat)
{
    int i;
    pat->seed = 1u;
    pat->time_length = ACID_MAX_STEPS;
    pat->pitch_length = 1;
    for (i = 0; i < ACID_MAX_STEPS; i++) {
        pat->time[i] = ACID_REST;
        pitch_zero(&pat->pitch[i]);
    }
    pat->time[0] = ACID_NOTE;
}
static void acid_sanitize(acid_pattern_t *pat)
{
    int i, s, e, notes = 0, active = 0, old_len;
    int8_t event_step[ACID_MAX_STEPS];
    pat->time_length = (uint8_t)ap_max(1, ap_min(pat->time_length, ACID_MAX_STEPS));
    for (i = 0; i < pat->time_length; i++) {
        if (pat->time[i] > ACID_TIE)
            pat->time[i] = ACID_REST;
        if (pat->time[i] == ACID_TIE && !active)
            pat->time[i] = ACID_REST;
        if (pat->time[i] == ACID_NOTE) {
            active = 1;
            notes++;
        } else if (pat->time[i] == ACID_REST)
            active = 0;
    }
    for (i = pat->time_length; i < ACID_MAX_STEPS; i++)
        pat->time[i] = ACID_REST;
    if (notes == 0) {
        pat->time[0] = ACID_NOTE;
        notes = 1;
    }
    old_len = ap_max(0, ap_min(pat->pitch_length, ACID_MAX_STEPS));
    for (i = old_len; i < notes; i++)
        pitch_zero(&pat->pitch[i]);
    pat->pitch_length = (uint8_t)notes;
    for (i = 0; i < pat->pitch_length; i++) {
        pat->pitch[i].degree = (int8_t)ap_max(-24, ap_min(pat->pitch[i].degree, 24));
        pat->pitch[i].octave = (int8_t)ap_max(-2, ap_min(pat->pitch[i].octave, 2));
    }
    for (i = pat->pitch_length; i < ACID_MAX_STEPS; i++)
        pitch_zero(&pat->pitch[i]);
    for (i = 0; i < ACID_MAX_STEPS; i++)
        event_step[i] = -1;
    e = 0;
    for (s = 0; s < pat->time_length && e < pat->pitch_length; s++)
        if (pat->time[s] == ACID_NOTE)
            event_step[e++] = (int8_t)s;
    for (i = 0; i < pat->pitch_length; i++) {         /* a slide needs the next step to attack */
        int st;
        if (!pat->pitch[i].slide_out)
            continue;
        st = event_step[i];
        if (st < 0 || pat->time[(st + 1) % pat->time_length] != ACID_NOTE)
            pat->pitch[i].slide_out = 0;
    }
}

/* ------------------------------------------------------------------ the RNG --- */
typedef struct { uint32_t state; } ap_rng_t;
static void ap_rng_init(ap_rng_t *r, uint32_t seed) { r->state = seed ? seed : 1u; }
static uint32_t ap_rng_next(ap_rng_t *r)
{
    r->state ^= r->state << 13;
    r->state ^= r->state >> 17;
    r->state ^= r->state << 5;
    return r->state;
}
static int32_t ap_unit(ap_rng_t *r) { return (int32_t)(ap_rng_next(r) >> 8); }   /* [0, 1) in Q24, exactly ATEK's */
static int ap_pick(ap_rng_t *r, int n) { return (int)(((int64_t)ap_unit(r) * n) >> 24) % n; }   /* (int)(u * n) % n */
static uint32_t ap_rng_mix(uint32_t seed, uint32_t stream)
{
    uint32_t z = seed ^ stream ^ 0x9e3779b9u;
    z ^= z >> 16;
    z *= 0x85ebca6bu;
    z ^= z >> 13;
    z *= 0xc2b2ae35u;
    z ^= z >> 16;
    return z ? z : 1u;
}
#define S_STYLE 0x5354594cu
#define S_TIME 0x54494d45u
#define S_PITCH 0x50495443u
#define S_OCTAVE 0x4f435441u
#define S_ACCENT 0x41434345u
#define S_SLIDE 0x534c4944u

/* -------------------------------------------------------------- generation --- */
/* the knobs at the SEQ's defaults: DENSITY 0.65, RANGE 0.45, ACCENT 0.6 (its curve 0.9 x 0.6^1.27), SLIDE 0.5, TIE 0.4 */
#define AP_DENSITY Q24(0.65f)
#define AP_TIE_CHANCE 3346048                  /* min(0.90, 2.77 x 0.072 x (0.4 / 0.4)), Q24 (float: 0.19944) */
#define AP_TIE_CHAIN 1171117                   /* .. x 0.35 after a tie */
#define AP_ACC_START 8287123                   /* min(0.95, 1.05 x the curve 0.47043) */
#define AP_ACC_REST 7497874                    /* min(0.90, 0.95 x the curve) */
#define AP_JUMP_START Q24(0.45f * 0.55f)       /* RANGE x 0.55 on a cell's first step, x 0.38 on the others */
#define AP_JUMP_REST Q24(0.45f * 0.38f)
#define AP_TWO 1161500                         /* clamp((RANGE - 0.35) / 0.65) x 0.45 */
#define AP_FOLD 23                             /* 12 + lround(RANGE x 24) semitones */

/* activation_threshold, Q24 (up to 1.43) */
static int32_t ap_activation(int32_t u, int cell_start)   /* (x 1 / slope: no 64-bit divide on the chip) */
{
    int32_t fl = cell_start ? Q24(0.30f) : Q24(0.05f);
    int64_t inv = cell_start ? 23967451 : 20971520;          /* 2^24 / 0.70, 2^24 / 0.80 */
    return u <= fl ? 0 : (int32_t)(((int64_t)(u - fl) * inv) >> 24);
}
static int choose_vocab_size(int32_t u, int scale_size)
{
    int n = VOCAB_MAX, i;
    for (i = 0; i < VOCAB_MAX; i++)
        if (u < VOCAB_SIZE_CDF[i]) {
            n = i + 1;
            break;
        }
    return n < scale_size ? n : scale_size;
}
static int build_vocab(int *voc, int scale, int want, ap_rng_t *rng)
{
    const acid_scale_t *sc = &ACID_SCALES[scale];
    uint8_t taken[12] = {0};
    int nv = 1, d, i, j;
    voc[0] = 0;
    taken[0] = 1;
    while (nv < want && nv < 12) {
        int32_t total = 0, r;
        for (d = 1; d < sc->n; d++)
            if (!taken[d])
                total += semi_weight(sc->s[d]);
        if (total <= 0)
            break;
        r = (int32_t)(((int64_t)ap_unit(rng) * total) >> 16);          /* (x 256: the weights' sum, Q8) */
        for (d = 1; d < sc->n; d++) {
            if (taken[d])
                continue;
            r -= semi_weight(sc->s[d]) << 8;
            if (r <= 0) {
                taken[d] = 1;
                voc[nv++] = d;
                break;
            }
        }
    }
    for (i = 1; i < nv; i++)
        for (j = i; j > 0 && voc[j - 1] > voc[j]; j--) {
            int t = voc[j];
            voc[j] = voc[j - 1];
            voc[j - 1] = t;
        }
    return nv;
}
static int pick_contour(ap_rng_t *rng)
{
    static const int32_t W[CONTOUR_LEN] = {Q24(0.22f), Q24(0.18f), Q24(0.15f), Q24(0.26f), Q24(0.19f)};
    int32_t r = ap_unit(rng);
    int i;
    for (i = 0; i < CONTOUR_LEN; i++) {
        r -= W[i];
        if (r <= 0)
            return i;
    }
    return C_PEDAL;
}
static void fold_pitch_range(acid_pattern_t *pat, int scale, int limit)
{
    int pass, i;
    for (pass = 0; pass < 6; pass++) {
        int lo = 999, hi = -999, mid, move_high, from, delta, moved = 0;
        for (i = 0; i < pat->pitch_length; i++) {
            int v = acid_semi_of(pat->pitch[i].degree, pat->pitch[i].octave, scale);
            lo = ap_min(lo, v);
            hi = ap_max(hi, v);
        }
        if (hi - lo <= limit)
            return;
        mid = (hi + lo) / 2;
        move_high = (hi - mid) >= (mid - lo);
        from = move_high ? hi : lo;
        delta = move_high ? -1 : 1;
        for (i = 0; i < pat->pitch_length; i++) {
            acid_pitch_t *p = &pat->pitch[i];
            int o;
            if (acid_semi_of(p->degree, p->octave, scale) != from)
                continue;
            o = p->octave + delta;
            if (o < -2 || o > 2)
                continue;
            p->octave = (int8_t)o;
            moved = 1;
        }
        if (!moved)
            return;
    }
}

/* a new line of length steps in scale (ACID_SCALES) from seed: acid_generate with the SEQ's default knobs */
static void acid_generate(acid_pattern_t *pat, int length, int scale, uint32_t seed)
{
    ap_rng_t style, time_rng, pitch_rng, octave_rng, accent_rng, slide_rng;
    int cell, contour, want_vocab, forced = 0, s, i, candidate_count = 0, vocab_length, event;
    int32_t activation[ACID_MAX_STEPS];
    uint8_t tie_plan[ACID_MAX_STEPS] = {0};
    int8_t candidate_at[ACID_MAX_STEPS], event_at[ACID_MAX_STEPS], vocab_shift[ACID_MAX_STEPS] = {0};
    int vocab[12], cell_vocab[4] = {0};
    acid_pitch_t candidates[ACID_MAX_STEPS];
    length = ap_max(1, ap_min(length, ACID_MAX_STEPS));
    scale = ap_max(0, ap_min(scale, ACID_NSCALE - 1));
    ap_rng_init(&style, ap_rng_mix(seed, S_STYLE));
    ap_rng_init(&time_rng, ap_rng_mix(seed, S_TIME));
    ap_rng_init(&pitch_rng, ap_rng_mix(seed, S_PITCH));
    ap_rng_init(&octave_rng, ap_rng_mix(seed, S_OCTAVE));
    ap_rng_init(&accent_rng, ap_rng_mix(seed, S_ACCENT));
    ap_rng_init(&slide_rng, ap_rng_mix(seed, S_SLIDE));
    acid_clear(pat);
    pat->seed = seed ? seed : 1u;
    pat->time_length = (uint8_t)length;
    cell = ap_unit(&style) < Q24(0.25f) ? 3 : 4;
    contour = pick_contour(&style);
    want_vocab = choose_vocab_size(ap_unit(&style), ACID_SCALES[scale].n);
    for (s = 0; s < length; s++) {
        activation[s] = ap_activation(ap_unit(&time_rng), s % cell == 0);
        if (activation[s] < activation[forced])
            forced = s;
    }
    for (s = 1; s < length; s++) {
        int32_t chance = tie_plan[s - 1] ? AP_TIE_CHAIN : AP_TIE_CHANCE;
        tie_plan[s] = activation[s - 1] <= activation[s] && ap_unit(&time_rng) < chance;
    }
    for (s = 0; s < length; s++) {
        int on = activation[s] <= AP_DENSITY || s == forced;
        pat->time[s] = !on ? ACID_REST : s > 0 && tie_plan[s] ? ACID_TIE : ACID_NOTE;
    }
    if (pat->time[forced] == ACID_TIE)
        pat->time[forced] = ACID_NOTE;
    for (i = 0; i < ACID_MAX_STEPS; i++)
        candidate_at[i] = -1;
    for (s = 0; s < length; s++) {
        int can_appear = activation[s] <= (1 << 24) || s == forced;
        int planned_tie = s > 0 && tie_plan[s] && s != forced;
        if (can_appear && !planned_tie)
            candidate_at[s] = (int8_t)candidate_count++;
    }
    pat->pitch_length = (uint8_t)candidate_count;
    vocab_length = build_vocab(vocab, scale, want_vocab, &pitch_rng);
    switch (contour) {
    case C_PEDAL:
        for (i = 0; i < cell; i++)
            cell_vocab[i] = (i == 0 || ap_unit(&pitch_rng) < Q24(0.45f)) ? 0 : ap_pick(&pitch_rng, vocab_length);
        break;
    case C_RISE:
    case C_FALL: {
        int a, b;
        for (i = 0; i < cell; i++)
            cell_vocab[i] = ap_pick(&pitch_rng, vocab_length);
        for (a = 0; a < cell; a++)
            for (b = a + 1; b < cell; b++)
                if (contour == C_RISE ? (vocab[cell_vocab[b]] < vocab[cell_vocab[a]])
                                      : (vocab[cell_vocab[b]] > vocab[cell_vocab[a]])) {
                    int t = cell_vocab[a];
                    cell_vocab[a] = cell_vocab[b];
                    cell_vocab[b] = t;
                }
        if (ap_unit(&pitch_rng) < Q24(0.6f))
            cell_vocab[0] = 0;
        break;
    }
    case C_ZIGZAG: {
        int top = vocab_length > 1 ? 1 + ap_pick(&pitch_rng, vocab_length - 1) : 0;
        for (i = 0; i < cell; i++)
            cell_vocab[i] = i % 2 ? top : 0;
        break;
    }
    default:
        cell_vocab[0] = ap_pick(&pitch_rng, vocab_length);
        for (i = 1; i < cell; i++)
            cell_vocab[i] = ap_unit(&pitch_rng) < Q24(0.40f) ? cell_vocab[i - 1] : ap_pick(&pitch_rng, vocab_length);
        break;
    }
    {
        int rep;
        for (rep = 1; rep * cell < ACID_MAX_STEPS; rep++)
            if (vocab_length > 1 && ap_unit(&pitch_rng) < Q24(0.45f)) {
                static const int8_t SHIFT[6] = {1, -1, 1, -1, 2, -2};
                vocab_shift[rep] = SHIFT[ap_pick(&pitch_rng, 6)];
            }
    }
    for (i = 0; i < ACID_MAX_STEPS; i++) {
        int cell_pos = i % cell, rep = i / cell, vi, degree;
        acid_pitch_t *p = &pat->pitch[i];
        vi = (cell_vocab[cell_pos] + vocab_shift[rep]) % vocab_length;
        if (vi < 0)
            vi += vocab_length;
        degree = vocab[vi];
        if (rep > 0 && ap_unit(&pitch_rng) < Q24(0.18f))
            degree = vocab[ap_pick(&pitch_rng, vocab_length)];
        p->degree = (int8_t)degree;
        if (ap_unit(&octave_rng) < (cell_pos == 0 ? AP_JUMP_START : AP_JUMP_REST)) {
            int amount = ap_unit(&octave_rng) < AP_TWO ? 2 : 1;
            p->octave = (int8_t)(ap_unit(&octave_rng) < Q24(0.55f) ? amount : -amount);
        }
        p->accent = ap_unit(&accent_rng) < (cell_pos == 0 ? AP_ACC_START : AP_ACC_REST);
        p->slide_out = 0;
    }
    fold_pitch_range(pat, scale, AP_FOLD);
    for (i = 0; i < ACID_MAX_STEPS; i++)
        candidates[i] = pat->pitch[i];
    pat->pitch_length = 0;
    for (s = 0; s < length; s++) {
        int c;
        if (pat->time[s] != ACID_NOTE)
            continue;
        c = candidate_at[s];
        if (c >= 0 && c < candidate_count)
            pat->pitch[pat->pitch_length++] = candidates[c];
    }
    for (i = pat->pitch_length; i < ACID_MAX_STEPS; i++)
        pitch_zero(&pat->pitch[i]);
    for (i = 0; i < ACID_MAX_STEPS; i++)
        event_at[i] = -1;
    event = 0;
    for (s = 0; s < length; s++)
        if (pat->time[s] == ACID_NOTE)
            event_at[s] = (int8_t)event++;
    for (s = 0; s < length; s++) {
        int here_e = event_at[s], next_step = (s + 1) % length, leap;
        acid_pitch_t *here;
        const acid_pitch_t *next;
        if (here_e < 0 || event_at[next_step] < 0)
            continue;
        here = &pat->pitch[here_e];
        next = &pat->pitch[event_at[next_step]];
        leap = ap_abs(acid_semi_of(next->degree, next->octave, scale) - acid_semi_of(here->degree, here->octave, scale));
        if (leap == 0)
            continue;
        here->slide_out = ap_unit(&slide_rng) < ap_min(Q24(0.95f), SLIDE_BY_LEAP[leap > 12 ? 12 : leap]);   /* (x 2 x SLIDE 0.5) */
    }
    acid_sanitize(pat);
}

/* ---------------------------------------------------------------- mutation --- */
static int ap_same(const acid_pattern_t *a, const acid_pattern_t *b)
{
    int i;
    if (a->time_length != b->time_length || a->pitch_length != b->pitch_length)
        return 0;
    for (i = 0; i < a->time_length; i++)
        if (a->time[i] != b->time[i])
            return 0;
    for (i = 0; i < a->pitch_length; i++)
        if (a->pitch[i].degree != b->pitch[i].degree || a->pitch[i].octave != b->pitch[i].octave
            || a->pitch[i].accent != b->pitch[i].accent || a->pitch[i].slide_out != b->pitch[i].slide_out)
            return 0;
    return 1;
}
static int event_index_at_step(const acid_pattern_t *pat, int wanted)
{
    int e = 0, s;
    for (s = 0; s < pat->time_length; s++) {
        if (pat->time[s] != ACID_NOTE)
            continue;
        if (s == wanted)
            return e;
        e++;
    }
    return -1;
}
static void remove_pitch(acid_pattern_t *pat, int e)
{
    int i;
    if (e < 0 || e >= pat->pitch_length)
        return;
    for (i = e; i + 1 < pat->pitch_length; i++)
        pat->pitch[i] = pat->pitch[i + 1];
    pat->pitch_length--;
    pitch_zero(&pat->pitch[pat->pitch_length]);
}
static void insert_pitch(acid_pattern_t *pat, int e, acid_pitch_t value)
{
    int i;
    if (pat->pitch_length >= ACID_MAX_STEPS)
        return;
    e = ap_max(0, ap_min(e, pat->pitch_length));
    for (i = pat->pitch_length; i > e; i--)
        pat->pitch[i] = pat->pitch[i - 1];
    pat->pitch[e] = value;
    pat->pitch_length++;
}
static int events_in_range(const acid_pattern_t *pat, int lo, int hi, int *out)
{
    int found = 0, ev = 0, s;
    for (s = 0; s < pat->time_length; s++) {
        if (pat->time[s] != ACID_NOTE)
            continue;
        if (s >= lo && s < hi && found < ACID_MAX_STEPS)
            out[found++] = ev;
        ev++;
    }
    return found;
}
static int mutate_time(acid_pattern_t *pat, ap_rng_t *rng, int lo, int hi)
{
    int length = pat->time_length, attempt, s, i, span = hi - lo;
    if (span <= 0)
        return 0;
    for (attempt = 0; attempt < 48; attempt++) {
        int from = lo + (int)(ap_rng_next(rng) % (uint32_t)span);
        int to = lo + (int)(ap_rng_next(rng) % (uint32_t)span);
        int after = (from + 1) % length;
        if (from == to || pat->time[from] != ACID_NOTE || pat->time[to] != ACID_REST || pat->time[after] == ACID_TIE)
            continue;
        pat->time[from] = ACID_REST;
        pat->time[to] = ACID_NOTE;
        return 1;
    }
    for (s = ap_max(1, lo); s < hi; s++) {
        int e = 0, prev;
        if (pat->time[s] != ACID_TIE)
            continue;
        for (i = 0; i < s; i++)
            if (pat->time[i] == ACID_NOTE)
                e++;
        prev = ap_max(0, e - 1);
        insert_pitch(pat, e, pat->pitch[prev]);
        pat->time[s] = ACID_NOTE;
        return 1;
    }
    for (s = ap_max(1, lo); s < hi; s++) {
        int e;
        if (pat->time[s] != ACID_NOTE || pat->time[s - 1] == ACID_REST)
            continue;
        e = event_index_at_step(pat, s);
        pat->time[s] = ACID_TIE;
        remove_pitch(pat, e);
        return 1;
    }
    return 0;
}
static int mutate_pitch(acid_pattern_t *pat, ap_rng_t *rng, int lo, int hi)
{
    acid_pitch_t *p;
    int cand[ACID_MAX_STEPS], n;
    if (!pat->pitch_length)
        return 0;
    n = events_in_range(pat, lo, hi, cand);
    if (n <= 0)
        return 0;
    p = &pat->pitch[cand[ap_rng_next(rng) % (uint32_t)n]];
    if (p->octave >= 2)
        p->octave--;
    else if (p->octave <= -2)
        p->octave++;
    else
        p->octave += (ap_rng_next(rng) & 1u) ? 1 : -1;
    return 1;
}
static int mutate_articulation(acid_pattern_t *pat, ap_rng_t *rng, int scale, int lo, int hi)
{
    int cand[ACID_MAX_STEPS], n, span = hi - lo;
    if (!pat->pitch_length)
        return 0;
    n = events_in_range(pat, lo, hi, cand);
    if (n <= 0 || span <= 0)
        return 0;
    if (ap_rng_next(rng) & 1u) {
        int event_at[ACID_MAX_STEPS], i, s, attempt, ev = 0;
        for (i = 0; i < ACID_MAX_STEPS; i++)
            event_at[i] = -1;
        for (s = 0; s < pat->time_length; s++)
            if (pat->time[s] == ACID_NOTE)
                event_at[s] = ev++;
        for (attempt = 0; attempt < span; attempt++) {
            int st = lo + (int)(ap_rng_next(rng) % (uint32_t)span), next = (st + 1) % pat->time_length;
            int a = event_at[st], b = event_at[next], here, there;
            if (a < 0 || b < 0)
                continue;
            here = acid_semi_of(pat->pitch[a].degree, pat->pitch[a].octave, scale);
            there = acid_semi_of(pat->pitch[b].degree, pat->pitch[b].octave, scale);
            if (here == there)
                continue;
            pat->pitch[a].slide_out = (uint8_t)!pat->pitch[a].slide_out;
            return 1;
        }
    }
    {
        int e = cand[ap_rng_next(rng) % (uint32_t)n];
        pat->pitch[e].accent = (uint8_t)!pat->pitch[e].accent;
    }
    return 1;
}
static int ap_mutate(acid_pattern_t *pat, int layer, uint32_t index, int scale)
{
    static acid_pattern_t before;                     /* (not on the main loop's stack: 320 bytes) */
    uint32_t tag = layer == ACID_MUT_TIME ? 0x4d54494du : layer == ACID_MUT_PITCH ? 0x4d504954u : 0x4d415254u;
    ap_rng_t rng;
    int changed, hi = pat->time_length;
    before = *pat;
    ap_rng_init(&rng, ap_rng_mix(pat->seed ^ (0x9e3779b9u * index), tag));
    if (layer == ACID_MUT_TIME)
        changed = mutate_time(pat, &rng, 0, hi);
    else if (layer == ACID_MUT_PITCH)
        changed = mutate_pitch(pat, &rng, 0, hi);
    else
        changed = mutate_articulation(pat, &rng, scale, 0, hi);
    acid_sanitize(pat);
    return changed && !ap_same(&before, pat);
}
/* a burst of operations mutations on one layer (acid_mutate_burst): 1 done, 0 nothing could change */
static int acid_mutate_burst(acid_pattern_t *pat, int layer, uint32_t first, int operations, int scale)
{
    static acid_pattern_t before;
    int wanted = ap_max(1, operations), applied = 0, attempt;
    before = *pat;
    for (attempt = 0; attempt < 32; attempt++) {
        if (ap_mutate(pat, layer, first + (uint32_t)attempt, scale))
            applied++;
        if (applied >= wanted && !ap_same(&before, pat))
            return 1;
    }
    *pat = before;
    return 0;
}
