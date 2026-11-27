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
    this.module = null;
    this.master = 820;
    this.hold = 0;            // after a cast's restart: 1 the splash runs, 2 held until CAST 1 (cast.c cast_boot_hold)
    this.holdT = 0;
    // (rc2) "realistic latency": a USB round trip of 10..20 ms, a flash write's 2 s (the unit's main loop stalls: one
    // frame waits in its buffer, the rest are lost), 1 % of frames lost each way. Seeded, so a test run repeats.
    this.real = false;
    this.seed = 1;
    this.outq = [];           // [at ms, bytes] in order
    this.busyUntil = 0;
    this.pending = null;      // the one frame the unit holds while busy
    this.lostIn = 0;
    this.lostOut = 0;
    this.port.onmessage = (e) => this.onMessage(e.data);
  }

  rnd() { this.seed = (this.seed * 1103515245 + 12345) >>> 0; return (this.seed >>> 8) / 16777216; }
  now() { return currentTime * 1000; }

  drain(extra = 0) {
    const ex = this.ex, n = ex.link_out_len();
    if (!n) return;
    const data = new Uint8Array(ex.memory.buffer, ex.link_out_buf(), n).slice();
    ex.link_out_clear();
    if (!this.real) { this.port.postMessage({ type: "midi", data }, [data.buffer]); return; }
    let s = -1;                                        // frame by frame: each may be lost, each comes a little later
    for (let i = 0; i < data.length; i++) {
      if (data[i] === 0xF0) s = i;
      else if (data[i] === 0xF7 && s >= 0) {
        if (this.rnd() < 0.01) this.lostOut++;
        else {
          const last = this.outq.length ? this.outq[this.outq.length - 1][0] : 0;
          this.outq.push([Math.max(last, this.now() + extra + 10 + 10 * this.rnd()), data.slice(s, i + 1)]);
        }
        s = -1;
      }
    }
  }
  flushOut() {
    const t = this.now();
    let n = 0;
    while (n < this.outq.length && this.outq[n][0] <= t) n++;
    if (!n) return;
    const parts = this.outq.splice(0, n).map((x) => x[1]);
    const data = new Uint8Array(parts.reduce((a, p) => a + p.length, 0));
    let o = 0;
    for (const p of parts) { data.set(p, o); o += p.length; }
    this.port.postMessage({ type: "midi", data }, [data.buffer]);
  }

  /** one frame into the unit's editor handler; a flash write meanwhile: the reply 2 s later (realistic mode) */
  take(d) {
    const ex = this.ex, n = d.length;
    if (n > 702) return;
    const w0 = ex.web_flash_writes_count();
    new Uint8Array(ex.memory.buffer, ex.link_in_buf(), n).set(d);
    ex.link_in(n);
    // a save (a project, a user sound, a backup object's commit, a sample's erase: several sectors) 2 s; a sample's
    // 256-byte piece (a page programmed, a sector erased now and then) 25 ms
    const w = this.real ? ex.web_flash_writes_count() - w0 : 0, busy = w >= 4 ? 2000 : w ? 25 : 0;
    if (busy) this.busyUntil = this.now() + busy;
    this.drain(busy);
  }

  async boot(nor, sectors) {
    const instance = await WebAssembly.instantiate(this.module, {});
    const ex = instance.exports;
    ex._initialize();
    ex.web_nor_erase();
    const mem = new Uint8Array(ex.memory.buffer, ex.web_nor(), ex.web_nor_size());
    if (nor) mem.set(nor); else restoreSectors(mem, sectors);
    ex.web_master(this.master);
    ex.web_boot();
    return ex;
  }

  /** CAST op 2 (cast.c): the unit stores its music and restarts; the virtual FM-1 starts again from its flash, its
   *  USB gone for a moment (the page sees the port go and come back), the splash held until the cast starts */
  async reboot() {
    const old = this.ex;
    this.ex = null;
    if (this.outq.length) {                            // (what the unit sent before its reset has gone: cast.c waits)
      for (const x of this.outq) x[0] = 0;
      this.flushOut();
    }
    this.port.postMessage({ type: "reboot" });
    const nor = new Uint8Array(old.memory.buffer, old.web_nor(), old.web_nor_size()).slice();
    this.outq = []; this.pending = null; this.busyUntil = 0;
    const ex = await this.boot(nor);
    this.sentWrites = ex.web_flash_writes_count();
    this.sentDraws = -1;
    this.hold = 1;
    this.holdT = this.now();
    this.ex = ex;
    this.port.postMessage({ type: "rebooted" });
  }

  async onMessage(m) {
    if (m.type === "load") {
      try {
        this.module = await WebAssembly.compile(m.wasm);
        this.master = m.master ?? 820;
        const ex = await this.boot(null, m.sectors);
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
    if (m.type === "config") {
      this.real = !!m.realistic;
      this.seed = m.seed || 1;
      return;
    }
    if (m.type === "stats") { this.port.postMessage({ type: "stats", lostIn: this.lostIn, lostOut: this.lostOut }); return; }
    if (m.type === "sysex") {
      if (this.real) {
        if (this.rnd() < 0.01) { this.lostIn++; return; }
        if (this.now() < this.busyUntil) {             // the main loop is in a flash write: one frame waits, the
          if (!this.pending) this.pending = m.data;     // rest are lost (the unit's single frame buffer)
          else this.lostIn++;
          return;
        }
      }
      this.take(m.data);
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
    if (this.real) {
      this.flushOut();
      if (this.pending && this.now() >= this.busyUntil) { const d = this.pending; this.pending = null; this.take(d); }
    }
    const out = outputs[0], n = out[0].length;
    if (this.hold) {                                   // (a cast's restart) the splash, then held for the cast
      if (this.hold === 1) {
        const at = ex.link_boot_ms();
        while (at && ex.web_now_ms() + 1 < at) ex.web_render(1);
        this.hold = 2;
      }
      ex.link_service();
      this.drain();
      if (!ex.link_cast_on() && this.now() - this.holdT < 10000) return true;
      this.hold = 0;
    }
    if (this.queue.length) this.input();
    ex.web_render(n);
    ex.link_service();
    this.drain();
    if (ex.link_reboot && ex.link_reboot()) { this.reboot(); return true; }
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
