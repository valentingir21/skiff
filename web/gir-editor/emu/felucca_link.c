/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments; Gir Editor (2026) */
/* (Gir Editor) a "virtual FM-1" for the editor: the web build Try Gir runs (web/emu/felucca_web.c: the whole firmware
 * against a simulated FM-1) plus the firmware's own SysEx editor handler (firmware/src/editor.c), its replies and
 * pushes captured instead of going to USB. The editor's EmulatorTransport sends the very frames it sends a real unit
 * over Web MIDI, and reads back what the real unit would answer: the same code answers both.
 * After web/emu/cast_follow.c, which runs editor.c behind the same build (its replies dropped).
 *   link_in_buf(): 702 bytes for one frame, F0 .. F7 included; link_in(n) hands it to ed_service (one reply or none)
 *   link_service(): the main loop's ed_service pass without a frame (the WATCH pushes), called after each render
 *   link_out_buf() / link_out_len() / link_out_clear(): what the device sent since the last clear, raw MIDI bytes
 * Built by web/gir-editor/emu/build.sh as felucca_link.wasm. */
#ifndef FELUCCA_CAST
#define FELUCCA_CAST 1                              /* (as a unit: the cast, cmd 83, and PANEL, cmd 84, the Screen view's */
#endif                                              /* panel and the SEQ TOOLS buttons; core.h FELUCCA_PANEL) */
#define GFX_HOOKS 0
#define GFX_HOOK_TEXT(x0, y0, x1, y1, s, flags) ((void)0)
#define GFX_HOOK_BLIT(x, y, r0) ((void)0)
#define GFX_HOOK_BEGIN() ((void)0)
#define GFX_HOOK_PIXELS(n) ((void)0)
#define CAST_READ() ((void)0)
#include "../../emu/felucca_web.c"

static uint8_t lk_f[700];                          /* the frame for ed_service: the bytes between F0 and F7 */
static uint32_t lk_n;
static uint8_t lk_ready;
#define SXQ 64u
static uint32_t so_r, so_w;                        /* (usb.c's send queue: always room here) */
static int ota_frame_get(const uint8_t **p, uint32_t *n)
{
    if (!lk_ready)
        return 0;
    *p = lk_f;
    *n = lk_n;
    return 1;
}
static void ota_frame_done(void) { lk_ready = 0; }
static uint8_t lk_out[16384];                       /* replies and pushes, raw bytes, F0 .. F7 each */
static uint32_t lk_out_n;
static uint32_t lk_out_lost;
static int ota_wire_send(const uint8_t *p, uint32_t n)
{
    if (lk_out_n + n > sizeof lk_out) {
        lk_out_lost++;
        return -1;
    }
    memcpy(lk_out + lk_out_n, p, n);
    lk_out_n += n;
    return 0;
}
static void audio_silence(void) {}
static void fl_inval(uint32_t off, uint32_t n) { (void)off; (void)n; }
static int fl_erase4k(uint32_t off, uint32_t *took) { *took = 0; return st_erase(off); }
static int fl_write(uint32_t off, const void *p, uint32_t n) { return st_prog(off, p, n); }
static void lk_wdt_feed(void)                       /* (a flash write while playing stops the transport first: */
{                                                   /* ed_flash_stop waits for the audio ISR's stop, done here) */
    fm1_ms++;
    if (transport_req == 2u) {
        seq_stop();
        transport_req = 0;
    }
}
#define fm1_wdt_feed() lk_wdt_feed()
#include "../../../firmware/src/editor.c"
#undef fm1_wdt_feed
#if FELUCCA_CAST
#include "../../../firmware/src/cast.c"
static void web_cast_block(void) {}                /* (cast_follow.c's replay hook: nothing to replay here) */
#endif

EXPORT uint8_t *link_in_buf(void)
{
    static uint8_t in[702];
    return in;
}
/* one frame F0 .. F7 (n bytes) from the editor; 0 taken, 1 not an editor frame, 2 too long */
EXPORT int32_t link_in(uint32_t n)
{
    const uint8_t *in = link_in_buf();
    uint32_t i;
    if (n < 6u || in[0] != 0xF0u || in[n - 1u] != 0xF7u)
        return 1;
    if (n - 2u > sizeof lk_f)
        return 2;
    for (i = 0; i + 2u < n; i++)
        lk_f[i] = in[i + 1u];
    lk_n = n - 2u;
    lk_ready = 1;
    ed_service();
    lk_ready = 0;
    return 0;
}
EXPORT void link_service(void) { ed_service(); }
EXPORT uint8_t *link_out_buf(void) { return lk_out; }
EXPORT uint32_t link_out_len(void) { return lk_out_n; }
EXPORT void link_out_clear(void) { lk_out_n = 0; }
EXPORT uint32_t link_out_lost(void) { return lk_out_lost; }
