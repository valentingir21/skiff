// SPDX-License-Identifier: GPL-3.0-only
// (plugin) The page inside the Gir Editor plugin (AU / VST3, plugin/): JUCE's WebBrowserComponent, a WKWebView.
//   page -> plugin: window.__JUCE__.backend.emitEvent("gir", {type, …})
//   plugin -> page: window.__girRecv({type, …})
// The plugin says who it is in JUCE's initialisation data (window.__JUCE__.initialisationData.gir).

export interface PluginInit { instance: number; version: string; host: string; format: string; autotest: string }
export interface PluginMsg { type: string; [k: string]: unknown }

interface Juce { backend: { emitEvent(id: string, o: unknown): void }; initialisationData?: Record<string, unknown[]> }
const juce = (): Juce | undefined => (globalThis as unknown as { __JUCE__?: Juce }).__JUCE__;

/** the plugin's identity, or null outside the plugin (the browser, the desktop app) */
export const plugin: PluginInit | null = (() => {
  const d = juce()?.initialisationData?.gir;
  const v = Array.isArray(d) ? d[0] : d;
  return v && typeof v === "object" ? (v as PluginInit) : null;
})();

const subs = new Set<(m: PluginMsg) => void>();
const waiting = new Map<number, (m: PluginMsg) => void>();
let rid = 0;

if (plugin) {
  (globalThis as unknown as { __girRecv: (m: PluginMsg) => void }).__girRecv = (m) => {
    if (m.type === "reply" && typeof m.rid === "number") {
      const w = waiting.get(m.rid);
      waiting.delete(m.rid);
      w?.(m);
      return;
    }
    for (const s of [...subs]) { try { s(m); } catch (e) { console.error(e); } }
  };
}

export function send(m: PluginMsg) { juce()?.backend.emitEvent("gir", m); }
/** a request the plugin replies to */
export function request<R = PluginMsg>(m: PluginMsg, timeout = 8000): Promise<R> {
  const id = ++rid;
  return new Promise<R>((res, rej) => {
    const t = setTimeout(() => { waiting.delete(id); rej(new Error("plugin: no reply to " + m.type)); }, timeout);
    waiting.set(id, (r) => { clearTimeout(t); res(r as R); });
    send({ ...m, rid: id });
  });
}
export function on(cb: (m: PluginMsg) => void) { subs.add(cb); return () => { subs.delete(cb); }; }

export function hex(d: ArrayLike<number>): string {
  let s = "";
  for (let i = 0; i < d.length; i++) s += (d[i] < 16 ? "0" : "") + d[i].toString(16);
  return s;
}
export function unhex(s: string): Uint8Array {
  const b = new Uint8Array(s.length >> 1);
  for (let i = 0; i < b.length; i++) b[i] = parseInt(s.substr(i * 2, 2), 16);
  return b;
}
