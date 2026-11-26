// SPDX-License-Identifier: GPL-3.0-only
// Web MIDI (Chrome, Edge, Opera, the Electron app): the FM-1's "Felucca" port, SysEx on.
// The port pairs as the original editor and the installer find them (proto.js portPairs: an input and an output of
// the same name; "Felucca" first, then other names macOS or Windows may give the unit). The first pair that answers
// INFO wins (the protocol layer's probe).
import { portPairs } from "../legacy/proto.js";
import { listeners, TransportError, type Transport, type TransportState } from "./types";

export function hasWebMidi(): boolean {
  return typeof navigator !== "undefined" && typeof (navigator as Navigator).requestMIDIAccess === "function";
}

export class WebMidiTransport implements Transport {
  readonly kind = "webmidi" as const;
  name = "FM-1";
  state: TransportState = "idle";
  private access: MIDIAccess | null = null;
  private input: MIDIInput | null = null;
  private output: MIDIOutput | null = null;
  private msg = listeners<[Uint8Array]>();
  private st = listeners<[TransportState, string?]>();
  /** tried in turn by the protocol: candidates() then use(pair) */
  pairs: [MIDIInput, MIDIOutput][] = [];

  private set(s: TransportState, why?: string) { this.state = s; this.st.emit(s, why); }

  async access_(): Promise<MIDIAccess> {
    if (this.access) return this.access;
    if (!hasWebMidi()) throw new TransportError("nomidi");
    try {
      this.access = await navigator.requestMIDIAccess({ sysex: true });
    } catch (e) {
      throw new TransportError("denied", String((e as Error)?.message || e));
    }
    this.access.onstatechange = (e) => this.onPortChange(e as MIDIConnectionEvent);
    return this.access;
  }

  /** the unit's port pairs (none: no FM-1 plugged in, or another app holds it on Windows) */
  async candidates(): Promise<[MIDIInput, MIDIOutput][]> {
    const access = await this.access_();
    this.pairs = portPairs(access) as [MIDIInput, MIDIOutput][];
    return this.pairs;
  }

  async use(pair: [MIDIInput, MIDIOutput]) {
    this.detach();
    const [i, o] = pair;
    this.input = i; this.output = o; this.name = i.name || "FM-1";
    i.onmidimessage = (e) => { if (e.data) this.msg.emit(e.data); };
    try { await i.open?.(); await o.open?.(); } catch (e) {
      throw new TransportError("busy", String((e as Error)?.message || e));
    }
  }

  async connect() {
    this.set("connecting");
    const pairs = await this.candidates();
    if (!pairs.length) { this.set("error", "nodevice"); throw new TransportError("nodevice"); }
    await this.use(pairs[0]);
    this.set("connected");
  }

  markConnected() { this.set("connected"); }

  send(frame: number[] | Uint8Array) {
    if (!this.output) throw new Error("closed");
    this.output.send(frame instanceof Uint8Array ? frame : Uint8Array.from(frame));
  }

  onMessage(cb: (d: Uint8Array) => void) { return this.msg.add(cb); }
  onState(cb: (s: TransportState, why?: string) => void) { return this.st.add(cb); }

  /** unplugged: "lost"; plugged back: "idle" with why "back", so the app reconnects */
  private onPortChange(e: MIDIConnectionEvent) {
    const p = e.port;
    if (!p) return;
    if (this.input && p.state === "disconnected" && (p.id === this.input.id || p.id === this.output?.id)) {
      this.detach();
      this.set("lost");
    } else if (this.state === "lost" && p.state === "connected" && p.type === "output" && /felucca|fm-1/i.test(p.name || "")) {
      this.set("idle", "back");
    }
  }

  private detach() {
    if (this.input) this.input.onmidimessage = null;
    this.input = null; this.output = null;
  }

  async disconnect() {
    this.detach();
    this.set("idle");
  }
}
