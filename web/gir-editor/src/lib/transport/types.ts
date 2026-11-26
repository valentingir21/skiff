// SPDX-License-Identifier: GPL-3.0-only
// The one interface every way of reaching an FM-1 implements: Web MIDI (Chrome, Edge, the desktop app), the virtual
// FM-1 (the firmware in WebAssembly), and later a plugin host. The protocol layer above sees only this.

export type TransportKind = "webmidi" | "emulator" | "plugin";
export type TransportState = "idle" | "connecting" | "connected" | "lost" | "error";

export interface Transport {
  readonly kind: TransportKind;
  /** a human name: the MIDI port's ("Felucca") or "Virtual FM-1" */
  readonly name: string;
  readonly state: TransportState;
  connect(): Promise<void>;
  /** one complete SysEx frame F0 .. F7 (the protocol builds it) */
  send(frame: number[] | Uint8Array): void;
  /** raw bytes from the device: SysEx frames (and, from a real unit, its notes and clock, which the protocol ignores) */
  onMessage(cb: (data: Uint8Array) => void): () => void;
  onState(cb: (s: TransportState, why?: string) => void): () => void;
  disconnect(): Promise<void>;
}

/** what goes wrong when connecting, each with a message key the UI explains (i18n "err.*") */
export type ConnectError =
  | "nomidi"          // this browser has no Web MIDI (Safari, Firefox)
  | "denied"          // MIDI or SysEx permission refused
  | "nodevice"        // no FM-1 port found, or none answered
  | "busy"            // a port found but another app holds it (Windows)
  | "noaudio"         // the demo needs an AudioContext
  | "wasm";           // the virtual FM-1 did not load

export class TransportError extends Error {
  constructor(public code: ConnectError, message?: string) {
    super(message || code);
  }
}

export function listeners<T extends unknown[]>() {
  const set = new Set<(...a: T) => void>();
  return {
    add(cb: (...a: T) => void) { set.add(cb); return () => { set.delete(cb); }; },
    emit(...a: T) { for (const cb of [...set]) { try { cb(...a); } catch (e) { console.error(e); } } },
  };
}
