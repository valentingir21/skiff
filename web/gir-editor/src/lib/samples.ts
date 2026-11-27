// SPDX-License-Identifier: GPL-3.0-only
// The user sample slots' tools, after the original editor's Samples tab (web/editor.html "samples"): decoding a file
// (its own WAV reader, else the browser's), the recording from the computer's input (raw: no echo cancelling, noise
// suppression or gain control), the preview as it will be stored (22050 Hz), and (rc2) one file cut into a drum
// kit's lanes at its hits. The slot itself is built by the original's code (proto.js buildSlot, kitZones: the
// firmware's format, byte for byte).
import { SMP, LANES, parseWav, resample, takeSample, autoTrim, rootFromName } from "./legacy/proto.js";

export interface Sound { fname: string; raw: Float64Array; a: number; b: number; root: number; mix: number[]; lvl: number }
export const RATE: number = SMP.RATE;
export const MAX_DATA: number = SMP.MAX_DATA;
export const MAX_ZONES: number = SMP.MAX_ZONES;
export const LANE_NAMES: string[] = LANES;
export const MIX0 = [127, 0, 0];
/** a sound's bytes in the slot (IMA ADPCM: 4 bits a sample) */
export const bytesOf = (s: Sound) => ((s.b - s.a) + 1) >> 1;
export const secondsOf = (bytes: number) => (bytes * 2) / RATE;
export const pctOf = (v: number) => Math.floor((v * 100 + 63) / 127);
export const rawOf = (p: number) => Math.floor((p * 127 + 50) / 100);

export async function decode(f: File | { name: string; arrayBuffer(): Promise<ArrayBuffer> }): Promise<Sound> {
  const buf = await f.arrayBuffer();
  let x: Float64Array;
  try { const w = parseWav(buf); x = resample(w.x, w.sr, RATE); }
  catch {                                            // not a plain WAV: the browser decodes it
    const ab = await new OfflineAudioContext(1, 1, RATE).decodeAudioData(buf.slice(0));
    x = new Float64Array(ab.length);
    for (let c = 0; c < ab.numberOfChannels; c++) { const d = ab.getChannelData(c); for (let i = 0; i < d.length; i++) x[i] += d[i] / ab.numberOfChannels; }
  }
  if (!x.length) throw new Error("no audio");
  const raw = Float64Array.from(x);
  return { fname: f.name, raw, a: 0, b: raw.length, root: rootFromName(f.name.replace(/\.[^.]*$/, "")), mix: MIX0.slice(), lvl: 1 };
}
/** the slot's name from a file's: 8 printable capitals */
export const slotName = (fname: string) => fname.replace(/\.[^.]*$/, "").toUpperCase().replace(/[^\x20-\x7E]/g, "").slice(0, 8);
export const sample = (s: Sound) => takeSample(s.raw, s.a, s.b) as Int16Array;
export { autoTrim };

// ------------------------------------------------------------------ preview ---
let pctx: AudioContext | null = null, psrc: AudioBufferSourceNode | null = null;
export function preview(s: Sound) {
  try {
    psrc?.stop();
    pctx = pctx || new AudioContext();
    pctx.resume().catch(() => {});
    const x = sample(s), buf = pctx.createBuffer(1, Math.max(1, x.length), RATE), ch = buf.getChannelData(0);
    for (let i = 0; i < x.length; i++) ch[i] = (x[i] / 32768) * s.lvl;
    psrc = pctx.createBufferSource();
    psrc.buffer = buf;
    psrc.connect(pctx.destination);
    psrc.start();
  } catch { /* no audio output */ }
}
export function stopPreview() { try { psrc?.stop(); } catch { /* ended */ } psrc = null; }

// ---------------------------------------------------------------- recording ---
const REC_WORKLET = `registerProcessor("gir-rec", class extends AudioWorkletProcessor {
  process(inputs) {
    const i = inputs[0];
    if (i && i.length && i[0].length) {
      const n = i[0].length, m = new Float32Array(n);
      for (let c = 0; c < i.length; c++) for (let k = 0; k < n; k++) m[k] += i[c][k] / i.length;
      this.port.postMessage(m, [m.buffer]);
    }
    return true;
  }
});`;
export async function inputs(): Promise<MediaDeviceInfo[]> {
  try { return (await navigator.mediaDevices.enumerateDevices()).filter((d) => d.kind === "audioinput"); } catch { return []; }
}
/** a recording: up to maxSec seconds (the slot's room), the level as it comes (peak 0..1), stop() -> the sound */
export class Recorder {
  ctx: AudioContext | null = null;
  stream: MediaStream | null = null;
  chunks: Float32Array[] = [];
  n = 0;
  max = 0;
  peak = 0;
  done = false;
  onFull: () => void = () => {};
  get seconds() { return this.ctx ? this.n / this.ctx.sampleRate : 0; }
  get maxSeconds() { return this.ctx ? this.max / this.ctx.sampleRate : 0; }

  async start(deviceId: string, maxSec: number) {
    this.stream = await navigator.mediaDevices.getUserMedia({ audio: {
      deviceId: deviceId ? { exact: deviceId } : undefined, echoCancellation: false, noiseSuppression: false, autoGainControl: false } });
    const ctx = (this.ctx = new AudioContext());
    await ctx.resume();
    const url = URL.createObjectURL(new Blob([REC_WORKLET], { type: "text/javascript" }));
    try { await ctx.audioWorklet.addModule(url); } finally { URL.revokeObjectURL(url); }
    const node = new AudioWorkletNode(ctx, "gir-rec"), mute = ctx.createGain();
    mute.gain.value = 0;                            // pulled by the graph, nothing heard
    ctx.createMediaStreamSource(this.stream).connect(node).connect(mute).connect(ctx.destination);
    this.max = Math.floor(maxSec * ctx.sampleRate);
    node.port.onmessage = (e) => {
      if (this.done) return;
      const d = e.data as Float32Array, c = d.subarray(0, Math.max(0, this.max - this.n));
      this.chunks.push(c);
      this.n += c.length;
      for (const v of d) this.peak = Math.max(this.peak, Math.abs(v));
      if (this.n >= this.max) this.onFull();
    };
  }
  /** the recording, trimmed to its sound; null: nothing came in (a muted or wrong input) */
  async stop(name: string): Promise<Sound | null> {
    this.done = true;
    this.stream?.getTracks().forEach((t) => t.stop());
    const sr = this.ctx?.sampleRate ?? RATE;
    await this.ctx?.close().catch(() => {});
    const x = new Float64Array(Math.min(this.n, this.max));
    let o = 0;
    for (const c of this.chunks) { const k = Math.min(c.length, x.length - o); x.set(c.subarray(0, k), o); o += k; }
    const raw = Float64Array.from(resample(x, sr, RATE));
    let pk = 0;
    for (const v of raw) pk = Math.max(pk, Math.abs(v));
    if (pk < 1e-4 || raw.length < 2) return null;
    const [a, b] = autoTrim(raw);
    return { fname: name, raw, a, b, root: 60, mix: MIX0.slice(), lvl: 1 };
  }
  cancel() { this.done = true; this.stream?.getTracks().forEach((t) => t.stop()); this.ctx?.close().catch(() => {}); }
}

// ---------------------------------------------------------------- slicing ---
/** (rc2) one file's hits: where the level jumps (a 5 ms envelope against the 50 ms before it), at least 60 ms apart,
 *  the strongest first if there are more than `max` -> the starts, in order */
export function onsets(raw: Float64Array, max = 8): number[] {
  const win = Math.round(RATE * 0.005), n = Math.floor(raw.length / win);
  const env = new Float64Array(n);
  let pk = 1e-9;
  for (let i = 0; i < n; i++) {
    let m = 0;
    for (let j = i * win; j < (i + 1) * win; j++) m = Math.max(m, Math.abs(raw[j]));
    env[i] = m; pk = Math.max(pk, m);
  }
  const found: { at: number; jump: number }[] = [];
  let last = -1e9;
  for (let i = 1; i < n; i++) {
    let before = 0;
    for (let j = Math.max(0, i - 10); j < i; j++) before = Math.max(before, env[j]);
    const jump = env[i] - before;
    if (env[i] > pk * 0.12 && jump > pk * 0.08 && (i - last) * win > RATE * 0.06) { found.push({ at: i * win, jump }); last = i; }
  }
  if (!found.length) return [0];
  const kept = found.length > max ? [...found].sort((a, b) => b.jump - a.jump).slice(0, max) : found;
  return kept.map((x) => Math.max(0, x.at - Math.round(RATE * 0.002))).sort((a, b) => a - b);
}
/** one file into the kit's lanes, in the lanes' order (KICK, SNARE, …): each hit up to the next (its tail trimmed) */
export function sliceToLanes(src: Sound, lanes = LANE_NAMES.length): (Sound | null)[] {
  const st = onsets(src.raw, lanes);
  const out: (Sound | null)[] = new Array(lanes).fill(null);
  st.forEach((a, i) => {
    const end = i + 1 < st.length ? st[i + 1] : src.raw.length;
    const seg = src.raw.subarray(a, end);
    const [, tb] = autoTrim(seg, 0.003);
    out[i] = { fname: `${src.fname} · ${i + 1}`, raw: src.raw, a, b: Math.min(end, a + Math.max(tb, Math.round(RATE * 0.02))), root: 60, mix: MIX0.slice(), lvl: 1 };
  });
  return out;
}
