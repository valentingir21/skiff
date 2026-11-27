// SPDX-License-Identifier: GPL-3.0-only
// (plugin) The FM-1 through the plugin's own CoreMIDI ports (plugin/src/MidiLink.cpp): WKWebView has no Web MIDI. As
// WebMidiTransport: the unit's port pairs (an input and an output of the same name, "Felucca" first), tried in turn by
// the protocol's INFO probe. One editor holds the port at a time (plugin/src/PortLock.cpp, shared with the other
// instances and the desktop app): opening a pair another one holds fails with "held" (TransportError detail: where),
// unless forced (taking over); the one that lost it hears "taken".
import { UNIT_PORT } from "../legacy/proto.js";
import { listeners, TransportError, type Transport, type TransportState } from "./types";
import { on, request, send, hex, unhex } from "../plugin/bridge";

export interface PortInfo { id: string; name: string }
export type PluginPair = [PortInfo, PortInfo];

export class PluginTransport implements Transport {
  readonly kind = "plugin" as const;
  name = "FM-1";
  state: TransportState = "idle";
  pairs: PluginPair[] = [];
  /** (taking over) the next use() opens the port even if another editor holds it */
  force = false;
  /** (the tests) only the ports of this name */
  only: string | null = null;
  private open = false;
  private msg = listeners<[Uint8Array]>();
  private st = listeners<[TransportState, string?]>();
  private off: () => void;

  constructor() {
    this.off = on((m) => {
      if (m.type === "midi" && this.open) for (const f of m.frames as string[]) this.msg.emit(unhex(f));
      else if (m.type === "lost" && this.open) { this.open = false; this.set("lost"); }
      else if (m.type === "taken") { this.open = false; this.set("lost", "taken:" + String(m.by ?? "")); }
      else if (m.type === "devices" && this.state === "lost") {
        const outs = ((m.ports as { outputs: PortInfo[] })?.outputs ?? []);
        if (outs.some((o) => /felucca|fm-1/i.test(o.name) && (!this.only || o.name === this.only))) this.set("idle", "back");
      }
    });
  }

  private set(s: TransportState, why?: string) { this.state = s; this.st.emit(s, why); }

  async candidates(): Promise<PluginPair[]> {
    const r = await request<{ ports: { inputs: PortInfo[]; outputs: PortInfo[] } }>({ type: "ports" });
    const outs = r.ports.outputs, pairs: PluginPair[] = [];
    for (const i of r.ports.inputs) {
      if (!UNIT_PORT.test(i.name) || (this.only && i.name !== this.only)) continue;
      const o = outs.find((x) => x.name === i.name);
      if (o) pairs.push([i, o]);
    }
    this.pairs = pairs.sort((a, b) => Number(/felucca/i.test(b[0].name)) - Number(/felucca/i.test(a[0].name)));
    return this.pairs;
  }

  async use(pair: PluginPair) {
    this.open = false;
    const r = await request<{ ok: boolean; error?: string; by?: string; name?: string }>({ type: "open", input: pair[0].id, output: pair[1].id, force: this.force });
    if (!r.ok) throw new TransportError(r.error === "held" ? "held" : "busy", r.by || "");
    this.force = false;
    this.open = true;
    this.name = r.name || pair[0].name;
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
    if (!this.open) throw new Error("closed");
    send({ type: "send", data: hex(frame) });
  }

  onMessage(cb: (d: Uint8Array) => void) { return this.msg.add(cb); }
  onState(cb: (s: TransportState, why?: string) => void) { return this.st.add(cb); }

  async disconnect() {
    send({ type: "close" });                   // (also after a failed probe: the lock is let go)
    this.open = false;
    this.off();
    this.set("idle");
  }
}
