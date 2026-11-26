// SPDX-License-Identifier: GPL-3.0-only
// The virtual FM-1 in Node, for the unit tests: felucca_link.wasm (the Gir firmware with its editor handler) behind the
// Transport interface. Its clock runs ahead as the test asks (tick) or on a timer (start), as a unit's would.
import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { listeners, type Transport, type TransportState } from "../../src/lib/transport/types";

const HERE = dirname(fileURLToPath(import.meta.url));
const WASM = join(HERE, "../../public/emu/felucca_link.wasm");

export class NodeDevice implements Transport {
  readonly kind = "emulator" as const;
  readonly name = "Node FM-1";
  state: TransportState = "idle";
  ex: any = null;
  private msg = listeners<[Uint8Array]>();
  private st = listeners<[TransportState, string?]>();
  private timer: ReturnType<typeof setInterval> | null = null;
  frames: number[][] = [];

  async connect() {
    const { instance } = await WebAssembly.instantiate(readFileSync(WASM), {});
    const ex: any = instance.exports;
    ex._initialize(); ex.web_nor_erase(); ex.web_master(700); ex.web_boot();
    this.ex = ex;
    this.tick(1200);                       // past the splash
    this.state = "connected"; this.st.emit("connected");
  }
  private drain() {
    const ex = this.ex, n = ex.link_out_len();
    if (!n) return;
    const d = new Uint8Array(ex.memory.buffer, ex.link_out_buf(), n).slice();
    ex.link_out_clear();
    let s = -1;
    for (let i = 0; i < d.length; i++) {
      if (d[i] === 0xF0) s = i;
      else if (d[i] === 0xF7 && s >= 0) { const f = d.subarray(s, i + 1); this.frames.push([...f]); queueMicrotask(() => this.msg.emit(f)); s = -1; }
    }
  }
  /** the device's time runs on ms milliseconds (its audio rendered, its main loop and pushes run) */
  tick(ms: number) {
    const ex = this.ex;
    for (let k = 0; k < Math.ceil(ms * 44.1 / 128); k++) { ex.web_render(128); ex.link_service(); }
    this.drain();
  }
  start(everyMs = 5) { this.timer = setInterval(() => this.tick(everyMs), everyMs); }
  send(frame: number[] | Uint8Array) {
    const ex = this.ex, f = frame instanceof Uint8Array ? frame : Uint8Array.from(frame);
    new Uint8Array(ex.memory.buffer, ex.link_in_buf(), f.length).set(f);
    ex.link_in(f.length);
    this.drain();
  }
  onMessage(cb: (d: Uint8Array) => void) { return this.msg.add(cb); }
  onState(cb: (s: TransportState, why?: string) => void) { return this.st.add(cb); }
  async disconnect() { if (this.timer) clearInterval(this.timer); this.timer = null; this.state = "idle"; }
  /** press and release panel buttons / keys (labels: panel.c B_*; keys: F3 = bit 0) through the device's own scan */
  hold(buttons: number, keys = 0) { this.ex.web_buttons(buttons >>> 0); this.ex.web_keys(keys >>> 0); }
}
