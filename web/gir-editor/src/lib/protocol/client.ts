// SPDX-License-Identifier: GPL-3.0-only
// The protocol over any transport: the original editor's Link (one request in flight, the rest queued, a reply matched
// to its request, pushes apart) and its request builders and parsers (proto.js), typed here.
// Flow control: a knob turned fast sends a request keyed by its parameter; while one is in flight the queued one is
// replaced by the newest value (Link's `key`), so the MIDI link never carries more than one frame at a time and the
// device ends on the last value.
import { CMD, Link, parse, req, songReply } from "../legacy/proto.js";
import type { Transport } from "../transport/types";
import type { Info } from "./types";

export interface RqOpt { timeout?: number; retries?: number; key?: string; front?: boolean; quiet?: boolean }
export type Request = [number, number[]];
export interface Push { cmd: number; a: number[] }

export class Client {
  link: InstanceType<typeof Link>;
  info: Info | null = null;
  closed = false;
  sent = 0;
  received = 0;
  timeouts = 0;
  private off: () => void;

  constructor(public transport: Transport, handlers: { onPush: (f: Push) => void; onTimeout?: (cmd: number) => void; onGap?: () => void; onCast?: (a: number[]) => void; onSent?: (d: number[]) => void }) {
    this.link = new Link((d: number[]) => { this.sent++; handlers.onSent?.(d); transport.send(d); }, {
      timeout: transport.kind === "emulator" ? 400 : 300,
      onUnknown: () => {},
      onTimeout: (cmd: number) => { this.timeouts++; handlers.onTimeout?.(cmd); },
      onPush: (f: Push) => handlers.onPush(f),
      onGap: () => handlers.onGap?.(),
      onCast: (a: number[]) => handlers.onCast?.(a),
    });
    this.off = transport.onMessage((d) => { this.received++; this.link.receive(d); });
  }

  rq(r: Request, opt?: RqOpt): Promise<number[]> {
    if (this.closed) return Promise.reject(new Error("closed"));
    return this.link.request(r, opt);
  }

  /** a request and its parsed reply */
  async ask<T = any>(r: Request, opt?: RqOpt): Promise<T> {
    const a = await this.rq(r, opt);
    const p = parse[r[0]];
    if (!p) return a as T;
    if (r[0] === CMD.LIST || r[0] === CMD.LIST2) return songReply({ info: this.info }, r, a) as T;
    return p(a, this.info) as T;
  }

  get idle(): boolean { return this.link.idle; }

  close() {
    this.closed = true;
    this.off();
    this.link.close();
  }
}

export { CMD, req, parse };

/** NAMES, tolerant: Gir 1.0's reply stops at 600 bytes (editor.c ed_out), so ANALOG's 89 names arrive cut (the
 *  original editor's parser then throws and its connection fails). -> the names that came whole, and whether all did */
export function parseNamesTolerant(a: number[]): { engine: number; count: number; names: string[]; titles: string[]; complete: boolean } {
  let i = 0;
  const b = () => a[i++];
  const s = (): string | null => {
    let out = "";
    for (;;) {
      if (i >= a.length) return null;
      const c = a[i++];
      if (!c) return out;
      out += String.fromCharCode(c);
    }
  };
  const engine = b(), count = b(), names: string[] = [];
  for (let k = 0; k < count; k++) { const n = s(); if (n == null) break; names.push(n); }
  const titles: string[] = [];
  if (names.length === count) for (let k = 0; k < 2; k++) { const t = s(); if (t == null) break; titles.push(t); }
  return { engine, count, names, titles, complete: names.length === count && titles.length === 2 };
}
