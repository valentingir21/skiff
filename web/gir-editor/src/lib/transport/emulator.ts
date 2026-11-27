// SPDX-License-Identifier: GPL-3.0-only
// The virtual FM-1: the Gir firmware in WebAssembly (felucca_link.wasm), in an AudioWorklet, answering the very SysEx
// a real unit answers (its editor.c). Demo mode, the tests' device, and the desktop app's offline mode.
// Its flash (projects, user sounds, samples) is kept in IndexedDB, so the demo remembers what was saved.
import { kvGet, kvSet, kvDel } from "../store";
import { listeners, TransportError, type Transport, type TransportState } from "./types";

const FLASH_KEY = "demo-flash";
export interface EmuFrame { fb?: Uint16Array; leds?: number[]; playing?: number }

export class EmulatorTransport implements Transport {
  readonly kind = "emulator" as const;
  readonly name = "Virtual FM-1";
  state: TransportState = "idle";
  ctx: AudioContext | null = null;
  node: AudioWorkletNode | null = null;
  gain: GainNode | null = null;
  private msg = listeners<[Uint8Array]>();
  private st = listeners<[TransportState, string?]>();
  private fr = listeners<[EmuFrame]>();
  lastFb: Uint16Array | null = null;
  /** true when the virtual FM-1 started with an empty flash (its first time here): the demo music is written */
  fresh = false;
  private saveTimer: ReturnType<typeof setTimeout> | null = null;
  /** (rc2) unplugged by a test: nothing goes either way, the unit keeps running (an FM-1 on its battery) */
  unplugged = false;
  /** (rc2) the realistic link (?latency=1 or gir.latency): 10-20 ms a reply, 2 s a flash write, 1 % of frames lost */
  realistic = false;

  constructor(private base = import.meta.env.BASE_URL || "./") {}

  private set(s: TransportState, why?: string) { this.state = s; this.st.emit(s, why); }

  async connect() {
    this.set("connecting");
    try {
      const AC = window.AudioContext || (window as unknown as { webkitAudioContext: typeof AudioContext }).webkitAudioContext;
      if (!AC) throw new TransportError("noaudio");
      this.ctx = new AC({ sampleRate: 44100, latencyHint: "interactive" });
      const url = (p: string) => new URL(this.base + p, document.baseURI).href;
      await this.ctx.audioWorklet.addModule(url("emu/link-worklet.js"));
      const [wasm, sectors] = await Promise.all([
        fetch(url("emu/felucca_link.wasm")).then((r) => { if (!r.ok) throw new TransportError("wasm"); return r.arrayBuffer(); }),
        kvGet<Record<string, Uint8Array>>(FLASH_KEY),
      ]);
      this.fresh = !sectors || !Object.keys(sectors).length;
      this.node = new AudioWorkletNode(this.ctx, "gir-link", { numberOfInputs: 0, outputChannelCount: [2] });
      this.gain = this.ctx.createGain();
      this.gain.gain.value = 0.8;
      this.node.connect(this.gain).connect(this.ctx.destination);
      const ready = new Promise<void>((res, rej) => {
        this.node!.port.onmessage = (e) => {
          const m = e.data;
          if (m.type === "ready") res();
          else if (m.type === "error") rej(new TransportError("wasm", m.message));
          else this.onWorklet(m);
        };
      });
      this.node.port.postMessage({ type: "load", wasm, sectors: sectors || {} }, [wasm]);
      await ready;
      this.node.port.onmessage = (e) => this.onWorklet(e.data);
      this.realistic = EmulatorTransport.wantRealistic();
      if (this.realistic) this.node.port.postMessage({ type: "config", realistic: true, seed: 7 });
      if (this.ctx.state === "suspended") await this.ctx.resume().catch(() => {});
      document.addEventListener("visibilitychange", this.onVis);
      this.set("connected");
    } catch (e) {
      this.set("error", String((e as Error).message));
      throw e instanceof TransportError ? e : new TransportError("wasm", String((e as Error).message));
    }
  }

  private onVis = () => this.node?.port.postMessage({ type: "visible", on: !document.hidden });

  static wantRealistic(): boolean {
    try {
      return new URLSearchParams(location.search).get("latency") === "1" || localStorage.getItem("gir.latency") === "1";
    } catch { return false; }
  }

  private onWorklet(m: { type: string; data?: Uint8Array; fb?: Uint16Array; leds?: number[]; playing?: number; sectors?: Record<string, Uint8Array>; lostIn?: number; lostOut?: number }) {
    if (m.type === "reboot") { this.set("lost", "reboot"); return; }        // CAST op 2: the unit restarts
    if (m.type === "rebooted") { if (!this.unplugged) setTimeout(() => this.set("idle", "back"), 300); return; }
    if (m.type === "stats") { this.statsCb?.(m as { lostIn: number; lostOut: number }); return; }
    if (m.type === "midi" && m.data) {
      if (this.unplugged) return;
      // the device's bytes since the last drain: whole frames F0 .. F7, one after the other
      const d = m.data;
      let s = -1;
      for (let i = 0; i < d.length; i++) {
        if (d[i] === 0xF0) s = i;
        else if (d[i] === 0xF7 && s >= 0) { this.msg.emit(d.subarray(s, i + 1)); s = -1; }
      }
    } else if (m.type === "frame") {
      if (m.fb) this.lastFb = m.fb;
      this.fr.emit({ fb: m.fb, leds: m.leds, playing: m.playing });
      if (m.sectors) {
        const sec = m.sectors;
        if (this.saveTimer) clearTimeout(this.saveTimer);
        this.saveTimer = setTimeout(() => kvSet(FLASH_KEY, sec), 300);
      }
    }
  }

  send(frame: number[] | Uint8Array) {
    if (!this.node) throw new Error("closed");
    if (this.unplugged) return;
    this.node.port.postMessage({ type: "sysex", data: frame instanceof Uint8Array ? frame : Uint8Array.from(frame) });
  }

  /** the panel, as the Screen view clicks it (the same inputs as Try Gir's) */
  panel(m: { type: "buttons" | "keys"; mask: number } | { type: "enc"; role: number; n: number }) {
    this.node?.port.postMessage(m);
  }
  setVolume(v: number) { if (this.gain) this.gain.gain.value = v; }
  async resume() { if (this.ctx?.state === "suspended") await this.ctx.resume().catch(() => {}); }

  onMessage(cb: (d: Uint8Array) => void) { return this.msg.add(cb); }
  onState(cb: (s: TransportState, why?: string) => void) { return this.st.add(cb); }
  onFrame(cb: (f: EmuFrame) => void) { return this.fr.add(cb); }

  async disconnect() {
    document.removeEventListener("visibilitychange", this.onVis);
    this.node?.disconnect();
    this.node = null;
    await this.ctx?.close().catch(() => {});
    this.ctx = null;
    this.set("idle");
  }

  /** (tests) the cable out: the editor loses the unit, which keeps running; and back in */
  unplug() { this.unplugged = true; this.set("lost", "unplug"); }
  replug() { this.unplugged = false; this.set("idle", "back"); }
  /** reconnecting to the same virtual FM-1 (after a replug or a cast's restart): it kept running */
  reattach() { if (this.node) this.set("connected"); }
  private statsCb: ((s: { lostIn: number; lostOut: number }) => void) | null = null;
  stats(): Promise<{ lostIn: number; lostOut: number }> {
    return new Promise((res) => { this.statsCb = res; this.node?.port.postMessage({ type: "stats" }); });
  }

  /** the demo's flash back to empty (a factory-fresh virtual FM-1) */
  static async resetFlash() { await kvDel(FLASH_KEY); }
}
