/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
#define FELUCCA_FAVORITES 1
/* Stable engine/preset references; fixed capacity independent of optional engines. */
typedef struct {
    uint8_t factory[16][32]; /* engine 0..15, preset 0..255 */
    uint32_t user, filter;
} favorites_t;
static favorites_t favorites;
/* (techno-engines) the row engine e's stars are kept in, *lim the presets it has room for (0: none). Engines 0..14 their
 * own row, as ever. The engines appended since (15 SUPERSAW .. 19 INDUS) cannot have rows of their own (row 15 is the
 * settings', the format has 16): they take the rows of the engines this build leaves out (LOFI 3, VOICE 5, WHEEL 7,
 * SLICE 13; their old stars cleared once at boot: ui.c unit_g_boot, ui_set_rev 4), INDUS row 1's first two bytes
 * (DIGITAL's, retired; its MIDI LEARN map starts at byte 2): 16 presets */
#ifdef NENG_GONE
#define FAV_FREE {!FELUCCA_LOFI, !FELUCCA_VOICE, !FELUCCA_WHEEL, !FELUCCA_SLICE, !FELUCCA_FM4}
#else
#define FAV_FREE {0, 0, 0, 0, 0}   /* (a host test that brings favorites.c without core.h: no borrowed rows) */
#endif
static uint32_t fav_row(uint32_t e, uint32_t *lim)
{
    static const uint8_t ROW[5] = {3, 5, 7, 13, 1};
    static const uint8_t FREE[5] = FAV_FREE;
    *lim = 256u;
    if (e < 15u)
        return e;
    if (e - 15u < 5u && FREE[e - 15u]) {
        if (ROW[e - 15u] == 1u)
            *lim = 16u;
        return ROW[e - 15u];
    }
    *lim = 0;
    return 0;
}
static int favorite_has(uint32_t engine, uint32_t preset)
{
    uint32_t lim, row;
    if (engine == NENGINES)
        return preset < UP_SLOTS && ((favorites.user >> preset) & 1u);
    row = fav_row(engine, &lim);
    return engine < NENGINES && preset < lim &&
        ((favorites.factory[row][preset / 8u] >> (preset % 8u)) & 1u);
}
static int favorite_set(uint32_t engine, uint32_t preset, int on)
{
    uint32_t lim, row = engine < NENGINES ? fav_row(engine, &lim) : 0u;
    if (engine > NENGINES || (engine == NENGINES ? preset >= UP_SLOTS : preset >= lim))
        return 0;
    if (favorite_has(engine, preset) == !!on) return 0;
    if (engine == NENGINES) {
        if (on) favorites.user |= 1u << preset;
        else favorites.user &= ~(1u << preset);
    } else {
        uint8_t *b = &favorites.factory[row][preset / 8u];
        if (on) *b |= (uint8_t)(1u << (preset % 8u));
        else *b &= (uint8_t)~(1u << (preset % 8u));
    }
    return 1;
}
