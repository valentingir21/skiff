// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments; Gir Editor (2026)
// Gir Editor's virtual FM-1 in an AudioWorklet: felucca_link.wasm is the Gir firmware against a simulated FM-1
// (web/emu/felucca_web.c) with its SysEx editor handler (firmware/src/editor.c). Each render quantum runs the device's
// clock forward 128 frames; the editor's SysEx frames go in between quanta (each answered at once, as the unit's main
// loop does), the replies and the WATCH pushes come back as MIDI bytes. About 30 times a second (4 hidden) the screen
// and the lights go out; a changed flash goes out whole, for the page to keep (the demo's projects survive a reload).
// After web/emu/worklet.js (Try Gir; after X0X's, charlesvestal/fm1-x0x, GPL-3.0).

const STORE_AREAS = [[0x97000, 0xE0000], [0xE5000, 0xE9000], [0xEA000, 0x100000]];
function storeSectors(nor) {
  const out = {};
  for (const [a, b] of STORE_AREAS)
    for (let off = a; off < b; off += 4096) {
      const s = nor.subarray(off, off + 4096);
      if (s.some((v) => v !== 0xFF)) out[off] = s.slice();
    }
  return out;
}
function restoreSectors(nor, sectors) {
  for (const [k, v] of Object.entries(sectors || {})) {
    const off = +k, b = v instanceof Uint8Array ? v : new Uint8Array(v);
    if (STORE_AREAS.some(([a, e]) => off >= a && off + 4096 <= e && off % 4096 === 0) && b.length === 4096) nor.set(b, off);
  }
}
const HOLD_MS = 24;

class GirLink extends AudioWorkletProcessor {
  constructor() {
    super();
    this.ex = null;
    this.sentDraws = -1;
    this.sentWrites = -1;
    this.sentLeds = "";
    this.lastFrame = 0;
    this.period = 1 / 30;
    this.queue = [];
    this.nextOk = 0;
    this.muted = false;
    this.port.onmessage = (e) => this.onMessage(e.data);
  }

  drain() {
    const ex = this.ex, n = ex.link_out_len();
    if (!n) return;
    const data = new Uint8Array(ex.memory.buffer, ex.link_out_buf(), n).slice();
    ex.link_out_clear();
    this.port.postMessage({ type: "midi", data }, [data.buffer]);
  }

  async onMessage(m) {
    if (m.type === "load") {
      try {
        const { instance } = await WebAssembly.instantiate(m.wasm, {});
        const ex = instance.exports;
        ex._initialize();
        ex.web_nor_erase();
        restoreSectors(new Uint8Array(ex.memory.buffer, ex.web_nor(), ex.web_nor_size()), m.sectors);
        ex.web_master(m.master ?? 820);
        ex.web_boot();
        this.ex = ex;
        this.sentWrites = ex.web_flash_writes_count();
        this.port.postMessage({ type: "ready", rate: ex.web_sample_rate() });
      } catch (err) {
        this.port.postMessage({ type: "error", message: String((err && err.message) || err) });
      }
      return;
    }
    const ex = this.ex;
    if (!ex) return;
    if (m.type === "sysex") {
      const d = m.data, n = d.length;
      if (n > 702) return;
      new Uint8Array(ex.memory.buffer, ex.link_in_buf(), n).set(d);
      ex.link_in(n);
      this.drain();
    } else if (m.type === "buttons" || m.type === "keys") this.queue.push(m);
    else if (m.type === "enc") ex.web_enc(m.role, m.n | 0);
    else if (m.type === "master") ex.web_master(m.value | 0);
    else if (m.type === "midi") ex.web_midi(m.data[0] | 0, m.data[1] | 0, m.data[2] | 0);
    else if (m.type === "visible") this.period = m.on ? 1 / 30 : 1 / 4;
    else if (m.type === "mute") this.muted = !!m.on;
  }

  publish() {
    const ex = this.ex, mem = ex.memory.buffer;
    const msg = { type: "frame" };
    const leds = [ex.web_lit_buttons(), ex.web_lit_keys(), ex.web_dim_buttons(), ex.web_dim_keys(), ex.web_dim_level(),
      ex.web_breath_buttons(), ex.web_breath_keys(), ex.web_mid_keys ? ex.web_mid_keys() : 0];
    const key = leds.join(",");
    if (key !== this.sentLeds) { this.sentLeds = key; msg.leds = leds; }
    msg.playing = ex.web_playing();
    const transfer = [];
    const draws = ex.web_screen_draws();
    if (draws !== this.sentDraws) {
      this.sentDraws = draws;
      msg.fb = new Uint16Array(mem, ex.web_screen(), 240 * 240).slice();
      transfer.push(msg.fb.buffer);
    }
    const writes = ex.web_flash_writes_count();
    if (writes !== this.sentWrites) {
      this.sentWrites = writes;
      msg.sectors = storeSectors(new Uint8Array(mem, ex.web_nor(), ex.web_nor_size()));
    }
    this.port.postMessage(msg, transfer);
  }

  input() {
    const ex = this.ex, now = ex.web_now_ms();
    while (this.queue.length && now - this.nextOk >= 0) {
      const m = this.queue.shift();
      if (m.type === "buttons") ex.web_buttons(m.mask >>> 0);
      else ex.web_keys(m.mask >>> 0);
      this.nextOk = now + HOLD_MS;
    }
  }

  process(inputs, outputs) {
    const ex = this.ex;
    if (!ex) return true;
    if (this.queue.length) this.input();
    const out = outputs[0], n = out[0].length;
    ex.web_render(n);
    ex.link_service();
    this.drain();
    if (!this.muted) {
      out[0].set(new Float32Array(ex.memory.buffer, ex.web_out_l(), n));
      if (out[1]) out[1].set(new Float32Array(ex.memory.buffer, ex.web_out_r(), n));
    }
    if (currentTime - this.lastFrame >= this.period) {
      this.lastFrame = currentTime;
      this.publish();
    }
    return true;
  }
}
registerProcessor("gir-link", GirLink);
