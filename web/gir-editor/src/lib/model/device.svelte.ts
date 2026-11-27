// SPDX-License-Identifier: GPL-3.0-only
// The FM-1 as the editor knows it, kept in sync both ways ("Live"): what is turned on the unit appears here (WATCH's
// pushes, a keep-alive poll), what is changed here is sent at once (one request in flight, a knob's values coalesced).
// Svelte 5 runes: every field the views read is reactive.
import { CMD, req, parse } from "../legacy/proto.js";
import { Client, parseNamesTolerant, type Push, type RqOpt } from "../protocol/client";
import type { Desc, Info, Lists, MenuItem, Perform, Pool, ProjNames, Step, TrackRow, UserSlot } from "../protocol/types";
import { EmulatorTransport } from "../transport/emulator";
import { WebMidiTransport } from "../transport/webmidi";
import { TransportError, type Transport } from "../transport/types";
import { ID, T, tdesc } from "./tables";
import { seedDemo } from "./demo";
import { prefs } from "../store";

export type Status = "off" | "connecting" | "loading" | "ready" | "lost" | "reconnecting";
export interface TrackState extends TrackRow { p: number[]; name: string }
export interface UndoEntry { label: string; track: number; scope: number; id: number; before: number; after: number; at: number }
export interface SoundUndo { label: string; track: number; before: { engine: number; preset: number; p: number[] }; after: { engine: number; preset: number }; at: number }
export interface StepsUndo { label: string; track: number; before: Record<number, Step>; after: Record<number, Step>; at: number }
type Entry = (UndoEntry & { kind: "param" }) | (SoundUndo & { kind: "sound" }) | (StepsUndo & { kind: "steps" });

const sleep = (ms: number) => new Promise((r) => setTimeout(r, ms));
/** SEQ TOOLS: white key place (from F3) -> its key bit (F3 = 0, chromatic) */
const WHITE_BIT = [0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26];
export const TOOL = { CLEAR: 0, REVERSE: 1, SHIFT_L: 2, SHIFT_R: 3, RANDOM: 4, COOK: 5, BEAT: 6, EUCLID: 7,
  LANE_CLR: 8, LANE_REV: 9, LANE_FILL: 10, LANE_RND: 11, DOUBLE: 12, HALF: 13,
  ACID_NEW: 8, ACID_RHYTHM: 9, ACID_NOTES: 10, ACID_ACCSLD: 11 } as const;
export const BTN = { FX: 0, SCL: 1, ENV: 2, LFO: 3, EDIT: 4, GLO: 5, HOME: 6, SAVE: 7, ARP: 8, SEQ: 9, PLAY: 10, REC: 11, OCTDN: 12, OCTUP: 13 };

export class Device {
  status = $state<Status>("off");
  kind = $state<"webmidi" | "emulator" | null>(null);
  portName = $state("");
  error = $state<{ code: string; detail?: string } | null>(null);
  progress = $state<{ what: string; pct: number } | null>(null);
  info = $state<Info | null>(null);
  /** true when the unit runs the firmware the tables were generated from (fast load; the help applies exactly) */
  tablesMatch = $state(false);
  tdesc = $state<Desc[]>([]);                 // track parameters 0 .. P_E0 - 1
  gdesc = $state<Desc[]>([]);                 // globals
  edesc = $state<Record<number, Desc[]>>({}); // per engine, P_E0 .. P_E0 + 7
  names = $state<string[][]>([]);             // preset names per engine
  titles = $state<string[][]>([]);
  tracks = $state<TrackState[]>([]);
  g = $state<number[]>([]);
  sel = $state(0);
  live = $state(false);                       // WATCH on: pushes arrive
  playing = $state(false);
  counting = $state(false);
  recMask = $state(0);
  steps = $state<Record<number, Step[]>>({});
  stepsLoading = $state<number | null>(null);
  lists = $state<Lists | null>(null);
  pool = $state<Pool | null>(null);
  perform = $state<Perform | null>(null);
  proj = $state<ProjNames | null>(null);
  userSlots = $state<UserSlot[]>([]);
  menu = $state<MenuItem[]>([]);
  smp = $state<any>(null);
  activity = $state<number[]>([0, 0, 0, 0, 0, 0]);   // last time (ms) each track changed or played
  undoStack = $state<Entry[]>([]);
  redoStack = $state<Entry[]>([]);
  lastPush = $state(0);
  stats = $state({ sent: 0, received: 0, timeouts: 0 });

  transport: Transport | null = null;
  client: Client | null = null;
  /** tests and the plugin host give their own transport */
  factory: ((kind: "webmidi" | "emulator") => Transport) | null = null;
  private alive: ReturnType<typeof setInterval> | null = null;
  private tickN = 0;
  private reloadPending = new Set<number>();
  private wantSong = false;
  private wantSteps: number | null = null;
  private listeners: Array<() => void> = [];
  onEvent: (e: { type: "toast"; level: "info" | "ok" | "warn" | "err"; key: string; vars?: Record<string, string | number> }) => void = () => {};

  get emu(): EmulatorTransport | null { return this.transport instanceof EmulatorTransport ? this.transport : null; }
  get ready() { return this.status === "ready"; }
  get pe0() { return this.info?.pe0 ?? T.P_E0; }
  get cur(): TrackState | undefined { return this.tracks[this.sel]; }
  engineName(e: number) { return this.info?.engines[e] ?? T.ENG[e]?.name ?? "?"; }
  /** the descriptor of track parameter id on engine e (EDIT's: that engine's) */
  desc(id: number, e: number): Desc | undefined {
    if (id >= this.pe0) return this.edesc[e]?.[id - this.pe0];
    return this.tdesc[id];
  }
  presetName(t: TrackState) { return this.names[t.engine]?.[t.preset] ?? ""; }

  // ------------------------------------------------------------------ connect ---
  async connect(kind: "webmidi" | "emulator") {
    if (this.status === "connecting" || this.status === "loading") return;
    await this.disconnect(true);
    this.error = null;
    this.status = "connecting";
    this.kind = kind;
    try {
      const tr: Transport = this.factory?.(kind) ?? (kind === "emulator" ? new EmulatorTransport() : new WebMidiTransport());
      this.transport = tr;
      this.listeners.push(tr.onState((s, why) => this.onTransportState(s, why)));
      if (tr instanceof WebMidiTransport) {
        await this.probe(tr);
      } else {
        await tr.connect();
        this.makeClient();
        if (tr instanceof EmulatorTransport) await sleep(450);   // the splash: the unit's main loop starts 430 ms after power-on
        this.info = await this.client!.ask(req.info(), { timeout: 600, retries: 3 });
      }
      this.portName = tr.name;
      await this.load();
    } catch (e) {
      const code = e instanceof TransportError ? e.code : /timeout/.test(String((e as Error).message)) ? "noreply" : "error";
      this.error = { code, detail: String((e as Error)?.message || e) };
      await this.disconnect(true);
      this.status = "off";
      throw e;
    }
  }

  private makeClient() {
    this.client = new Client(this.transport!, {
      onPush: (f) => this.onPush(f),
      onTimeout: () => { this.stats.timeouts++; },
      onGap: () => { if (this.live) this.rewatch(); },
    });
    this.client.info = this.info;
  }

  /** the first port pair that answers INFO (the installer's handshake); "busy": a pair seen but none would open */
  private async probe(tr: WebMidiTransport) {
    const pairs = await tr.candidates();
    if (!pairs.length) throw new TransportError("nodevice");
    let busy: TransportError | null = null;
    for (const pair of pairs) {
      try { await tr.use(pair); } catch (e) { busy = e as TransportError; continue; }
      this.makeClient();
      try {
        this.info = await this.client!.ask(req.info(), { timeout: 500, retries: 2, quiet: true });
        tr.markConnected();
        return;
      } catch { this.client!.close(); this.client = null; }
    }
    throw busy ?? new TransportError("noanswer");
  }

  /** why the unit went: "unplug" (or a port gone), "reboot" (the Screen view's cast restart) */
  lostWhy = $state("");
  /** (rc2) a write the unit was in when it went (a project save, a restore, a sample): said, and offered again */
  interrupted = $state<{ what: string; resume?: () => Promise<unknown> } | null>(null);
  private writingNow: { what: string; resume?: () => Promise<unknown> } | null = null;
  /** run a write to the unit's flash; if the unit goes meanwhile, it is remembered (interrupted) to say and resume */
  async writing<X>(what: string, fn: () => Promise<X>, resume?: () => Promise<unknown>): Promise<X> {
    this.writingNow = { what, resume };
    try { return await fn(); }
    catch (e) {
      if (this.status === "lost" || this.status === "reconnecting" || /closed/.test(String((e as Error)?.message))) {
        this.interrupted = { what, resume };
        throw new Error("interrupted");
      }
      throw e;
    } finally { this.writingNow = null; }
  }
  /** (the Screen view) called right after INFO when the unit comes back, before the editor's own reading */
  afterInfo: ((d: Device) => Promise<void> | void) | null = null;

  private onTransportState(s: string, why?: string) {
    if (s === "lost" && this.status !== "off") {
      this.status = "lost";
      this.lostWhy = why ?? "unplug";
      this.stopAlive();
      if (this.writingNow) this.interrupted = this.writingNow;
      this.client?.close();
      this.client = null;
      this.live = false;
      if (why !== "reboot") this.onEvent({ type: "toast", level: "warn", key: "toast.lost" });
    } else if (s === "idle" && why === "back" && this.status === "lost") {
      this.status = "reconnecting";
      setTimeout(() => this.reconnect().catch(() => { this.status = "lost"; }), this.kind === "emulator" ? 50 : 800);
    }
  }

  /** the same unit back (plugged in again, or restarted for the cast): the same transport, read again */
  async reconnect() {
    const tr = this.transport;
    if (!tr) return;
    this.status = "reconnecting";
    if (tr instanceof WebMidiTransport) await this.probe(tr);
    else {
      (tr as EmulatorTransport).reattach?.();
      this.makeClient();
      this.info = await this.client!.ask(req.info(), { timeout: 600, retries: 5 });
    }
    this.portName = tr.name;
    try { await this.afterInfo?.(this); } catch (e) { console.warn("afterInfo", e); }
    await this.load();
    if (this.lostWhy !== "reboot") this.onEvent({ type: "toast", level: "ok", key: "toast.back" });
  }

  async disconnect(quiet = false) {
    this.stopAlive();
    this.client?.close();
    this.client = null;
    for (const off of this.listeners.splice(0)) off();
    await this.transport?.disconnect().catch(() => {});
    this.transport = null;
    this.live = false;
    if (!quiet) { this.status = "off"; this.kind = null; }
  }

  rq(r: [number, number[]], opt?: RqOpt) {
    if (!this.client) return Promise.reject(new Error("closed"));
    return this.client.rq(r, opt);
  }
  ask<X = any>(r: [number, number[]], opt?: RqOpt): Promise<X> {
    if (!this.client) return Promise.reject(new Error("closed"));
    return this.client.ask<X>(r, opt);
  }

  // --------------------------------------------------------------------- load ---
  private async load() {
    this.status = "loading";
    const info = this.info!;
    this.client!.info = info;
    const same = info.pcount === T.P_COUNT && info.pe0 === T.P_E0 && info.gcount === T.G_COUNT &&
      info.engines.length === T.ENG.length && info.engines.every((n, i) => n === T.ENG[i].name);
    this.tablesMatch = same;
    if (same) {                                 // the published Gir 1.0: the tables are the device's own (tables.test.ts)
      this.tdesc = T.TP.map((d, i) => tdesc(d, 0, i));
      this.gdesc = T.GP.map((d, i) => tdesc(d, 1, i));
      const ed: Record<number, Desc[]> = {};
      T.ENG.forEach((e, k) => { ed[k] = e.edit.map((d, j) => tdesc(d, 0, T.P_E0 + j)); });
      this.edesc = ed;
      this.names = T.ENG.map((e) => e.presets.map((p) => p.name.slice(0, 12)));
      this.titles = T.ENG.map((e) => [...e.titles]);
    } else {                                    // another firmware: ask the device (slower, as the original editor)
      const total = info.pcount + info.gcount + info.nengines;
      let n = 0;
      const step = (what: string) => { this.progress = { what, pct: Math.round((100 * ++n) / total) }; };
      const td: Desc[] = [];
      for (let i = 0; i < info.pe0; i++) { step("desc"); td[i] = await this.ask(req.desc(0, i)).catch(() => null as any); }
      const gd: Desc[] = [];
      for (let i = 0; i < info.gcount; i++) { step("desc"); gd[i] = await this.ask(req.desc(1, i)).catch(() => null as any); }
      this.tdesc = td; this.gdesc = gd;
      const nm: string[][] = [], tt: string[][] = [];
      for (let e = 0; e < info.nengines; e++) {
        step("names");
        const r = parseNamesTolerant(await this.rq(req.names(e)));
        nm[e] = r.names; tt[e] = r.titles;
      }
      this.names = nm; this.titles = tt;
      this.edesc = {};
    }
    this.progress = { what: "tracks", pct: 60 };
    await this.readTracks(true);
    const cur = this.tracks[this.sel];
    if (cur) await this.readEngineDesc(cur.engine, !same);
    this.progress = { what: "state", pct: 85 };
    await this.readTransport().catch(() => {});
    await this.readProj().catch(() => {});
    const w = parse[CMD.WATCH](await this.rq(req.watch(3), { timeout: 400, retries: 1, quiet: true }).catch(() => [0]));
    this.live = !!(w.on & 1);
    const emu = this.transport instanceof EmulatorTransport ? this.transport : null;
    if (emu?.fresh && !prefs.get("noseed", false)) {           // the demo's first time: a loop to play with
      emu.fresh = false;
      try { await seedDemo(this, (pct) => (this.progress = { what: "demo", pct })); } catch (e) { console.warn("demo", e); }
    }
    this.progress = null;
    this.status = "ready";
    this.startAlive();
  }

  /** TRACK (every track's engine, preset, level, mute, armed) then TRACK_DUMP of each: all their parameters */
  async readTracks(full = false) {
    const tr = await this.ask<{ sel: number; ntrk: number; tracks: TrackRow[] }>(req.track());
    this.sel = tr.sel;
    const out: TrackState[] = [];
    for (let k = 0; k < tr.ntrk; k++) {
      const row = tr.tracks[k];
      let p = this.tracks[k]?.p;
      if (full || !p || this.tracks[k].engine !== row.engine || this.tracks[k].preset !== row.preset) {
        const d = await this.ask<{ p: number[] }>(req.trackDump(k));
        p = d.p;
      }
      out.push({ ...row, p: [...p], name: this.names[row.engine]?.[row.preset] ?? "" });
    }
    this.tracks = out;
    if (full) this.g = (await this.ask<{ g: number[] }>(req.dump())).g;
  }

  /** the EDIT descriptors of the selected track's engine, as the device has them now (SAMPLE's SET names: the user
   *  samples'); from the tables otherwise */
  async readEngineDesc(e: number, force = false) {
    if (!force && this.edesc[e] && T.ENG[e]?.name !== "SAMPLE") return;
    if (this.tracks[this.sel]?.engine !== e) return;
    const out: Desc[] = [];
    for (let k = 0; k < 8; k++) out.push(await this.ask(req.desc(0, this.pe0 + k)));
    this.edesc = { ...this.edesc, [e]: out };
  }

  // ------------------------------------------------------------------ live sync ---
  private onPush(f: Push) {
    this.lastPush = Date.now();
    const sel = this.sel;
    if (f.cmd === CMD.CHANGED) {
      const x = parse[CMD.CHANGED](f.a);
      if (x.scope === 0 && this.tracks[sel]) { this.tracks[sel].p[x.id] = x.value; this.touch(sel); this.mirrorMix(sel, x.id, x.value); }
      else if (x.scope === 1) this.g[x.id] = x.value;
    } else if (f.cmd === CMD.TRACK_CHANGED) {
      const x = parse[CMD.TRACK_CHANGED](f.a);
      if (this.tracks[x.track]) { this.tracks[x.track].p[x.id] = x.value; this.mirrorMix(x.track, x.id, x.value); this.touch(x.track); }
    } else if (f.cmd === CMD.RELOAD) {
      const x = parse[CMD.RELOAD](f.a);
      this.reloadPending.add(x.track);
      if (x.track !== this.sel) this.sel = x.track;
    } else if (f.cmd === CMD.STEP_CHANGED) {
      const x = parse[CMD.STEP_CHANGED](f.a);
      if (this.steps[x.track]) this.refreshStep(x.track, x.index);
    }
  }
  private mirrorMix(k: number, id: number, v: number) {
    const t = this.tracks[k];
    if (!t) return;
    if (id === ID.P_LEVEL) t.level = v;
    if (id === ID.P_MUTE) t.mute = v;
  }
  touch(k: number) { this.activity[k] = Date.now(); }

  private async refreshStep(k: number, i: number) {
    try { const s = await this.ask<Step>(req.trackStep(k, i)); if (this.steps[k]) this.steps[k][i] = s; } catch { /* next poll */ }
  }

  private startAlive() {
    this.stopAlive();
    this.alive = setInterval(() => this.keepAlive(), 500);
  }
  private stopAlive() { if (this.alive) clearInterval(this.alive); this.alive = null; }
  private busy = 0;
  /** every 0.5 s: what changed on the device (a load, the transport); TRANSPORT keeps WATCH alive (3 s) */
  private async keepAlive() {
    if (!this.client || this.status !== "ready" || this.busy) return;
    if (!this.client.idle) return;
    this.busy++;
    try {
      this.tickN++;
      if (this.reloadPending.size) {
        const ks = [...this.reloadPending]; this.reloadPending.clear();
        await this.readTracks();
        for (const k of ks) {
          const d = await this.ask<{ p: number[]; engine: number; preset: number }>(req.trackDump(k));
          if (this.tracks[k]) { this.tracks[k].p = d.p; this.tracks[k].engine = d.engine; this.tracks[k].preset = d.preset; this.tracks[k].name = this.names[d.engine]?.[d.preset] ?? ""; }
          if (this.steps[k]) this.loadSteps(k).catch(() => {});
        }
        const e = this.tracks[this.sel]?.engine;
        if (e != null) await this.readEngineDesc(e, T.ENG[e]?.name === "SAMPLE");
      }
      if (this.tickN % 2 === 0) await this.readTransport();
      if (this.tickN % 4 === 0) {          // levels, mutes, armed: the mixer's meters of activity
        const tr = await this.ask<{ sel: number; tracks: TrackRow[] }>(req.track());
        tr.tracks.forEach((r, k) => {
          const t = this.tracks[k];
          if (!t) return;
          if (t.engine !== r.engine || t.preset !== r.preset) this.reloadPending.add(k);
          t.level = r.level; t.mute = r.mute; t.armed = r.armed;
        });
        if (tr.sel !== this.sel) { this.sel = tr.sel; this.reloadPending.add(tr.sel); }
      }
      if (this.wantSong && this.tickN % 2 === 1) await this.readSong(this.tickN % 8 === 1);
      if (this.live && this.tickN % 5 === 0) await this.rq(req.watch(3), { quiet: true, timeout: 400 }).catch(() => {});
    } catch { /* a missed poll: the next one */ } finally { this.busy--; }
    if (this.client) this.stats = { sent: this.client.sent, received: this.client.received, timeouts: this.client.timeouts };
  }
  private async rewatch() { await this.rq(req.watch(3), { quiet: true }).catch(() => {}); }
  setSongWatch(on: boolean) { this.wantSong = on; if (on) this.readSong(true).catch(() => {}); }

  // ---------------------------------------------------------------- parameters ---
  private undoPush(e: Entry) {
    const last = this.undoStack[this.undoStack.length - 1];
    if (e.kind === "param" && last?.kind === "param" && last.track === e.track && last.scope === e.scope && last.id === e.id && e.at - last.at < 800) {
      last.after = e.after; last.at = e.at;               // a knob turned: one undo for the whole gesture
    } else {
      this.undoStack.push(e);
      if (this.undoStack.length > 200) this.undoStack.shift();
    }
    this.redoStack = [];
  }

  /** set track k's parameter id (scope 0) to v: optimistic, then the device's clamped value */
  async setParam(k: number, id: number, v: number, opt: { undo?: boolean; label?: string } = {}) {
    const t = this.tracks[k];
    if (!t || !this.client) return;
    const before = t.p[id];
    v = Math.round(v);
    if (before === v) return;
    t.p[id] = v;
    this.mirrorMix(k, id, v);
    if (opt.undo !== false) this.undoPush({ kind: "param", label: opt.label || "", track: k, scope: 0, id, before, after: v, at: Date.now() });
    try {
      const r = k === this.sel
        ? await this.ask<{ value: number }>(req.set(0, id, v), { key: `p:${k}:${id}` })
        : await this.ask<{ value: number }>(req.trackParam(k, id, v), { key: `p:${k}:${id}` });
      if (this.tracks[k] && this.tracks[k].p[id] === v && r.value !== v) this.tracks[k].p[id] = r.value;
    } catch (e) { this.fail(e); }
  }

  async setGlobal(id: number, v: number, opt: { undo?: boolean } = {}) {
    if (!this.client) return;
    const before = this.g[id];
    v = Math.round(v);
    if (before === v) return;
    this.g[id] = v;
    if (opt.undo !== false) this.undoPush({ kind: "param", label: "", track: -1, scope: 1, id, before, after: v, at: Date.now() });
    try {
      const r = await this.ask<{ value: number }>(req.set(1, id, v), { key: `g:${id}` });
      if (this.g[id] === v && r.value !== v) this.g[id] = r.value;
    } catch (e) { this.fail(e); }
  }

  async selectTrack(k: number) {
    if (k === this.sel || !this.client) return;
    this.sel = k;
    try {
      await this.ask(req.track(k));
      const e = this.tracks[k]?.engine;
      if (e != null) await this.readEngineDesc(e);
    } catch (e) { this.fail(e); }
  }

  /** load factory preset (engine e, preset p) on track k (selected first: PRESET acts on the selected track) */
  async loadPreset(k: number, e: number, p: number, opt: { undo?: boolean } = {}) {
    const t = this.tracks[k];
    if (!t || !this.client) return;
    const before = { engine: t.engine, preset: t.preset, p: [...t.p] };
    try {
      if (k !== this.sel) await this.selectTrack(k);
      const r = await this.ask<{ engine: number; preset: number }>(req.preset(e, p));
      const d = await this.ask<{ p: number[]; engine: number; preset: number }>(req.trackDump(k));
      this.tracks[k] = { ...this.tracks[k], engine: d.engine, preset: d.preset, p: d.p, name: this.names[d.engine]?.[d.preset] ?? "" };
      await this.readEngineDesc(d.engine);
      this.touch(k);
      if (opt.undo !== false) { this.undoStack.push({ kind: "sound", label: "", track: k, before, after: { engine: r.engine, preset: r.preset }, at: Date.now() }); this.redoStack = []; }
      if (r.engine !== e) this.onEvent({ type: "toast", level: "warn", key: "toast.engineRefused", vars: { engine: this.engineName(e), track: k + 1 } });
    } catch (err) { this.fail(err); }
  }

  /** a new engine on track k: its first preset, as the device's EDIT layer does */
  async setEngine(k: number, e: number) {
    return this.loadPreset(k, e, 0);
  }

  /** put a sound back exactly: its engine and preset, then every value that differs */
  private async restoreSound(k: number, s: { engine: number; preset: number; p: number[] }) {
    await this.loadPreset(k, s.engine, s.preset, { undo: false });
    const t = this.tracks[k];
    for (let id = 0; id < s.p.length; id++)
      if (t.p[id] !== s.p[id]) await this.setParam(k, id, s.p[id], { undo: false });
  }

  get canUndo() { return this.undoStack.length > 0; }
  get canRedo() { return this.redoStack.length > 0; }
  async undo() {
    const e = this.undoStack.pop();
    if (!e) return;
    if (e.kind === "param") {
      if (e.scope === 1) await this.setGlobal(e.id, e.before, { undo: false });
      else await this.setParam(e.track, e.id, e.before, { undo: false });
    } else if (e.kind === "steps") {
      for (const [i, st] of Object.entries(e.before)) await this.writeStep(e.track, +i, st);
    } else {
      const now = this.tracks[e.track];
      const redo = { engine: now.engine, preset: now.preset };
      await this.restoreSound(e.track, e.before);
      e.after = redo;
    }
    this.redoStack.push(e);
  }
  async redo() {
    const e = this.redoStack.pop();
    if (!e) return;
    if (e.kind === "param") {
      if (e.scope === 1) await this.setGlobal(e.id, e.after, { undo: false });
      else await this.setParam(e.track, e.id, e.after, { undo: false });
    } else if (e.kind === "steps") {
      for (const [i, st] of Object.entries(e.after)) await this.writeStep(e.track, +i, st);
    } else await this.loadPreset(e.track, e.after.engine, e.after.preset, { undo: false });
    this.undoStack.push(e);
  }

  // --------------------------------------------------------------------- mixer ---
  async setLevel(k: number, v: number) { return this.setParam(k, ID.P_LEVEL, v, { label: "LEVEL" }); }
  async setMute(k: number, on: boolean) { return this.setParam(k, ID.P_MUTE, on ? 1 : 0, { label: "MUTE" }); }
  /** solo: every other track muted (the editor's own: the device has no solo), again: all back as before */
  private soloBefore: number[] | null = null;
  soloTrack = $state<number | null>(null);
  async solo(k: number) {
    if (this.soloTrack === k && this.soloBefore) {
      const b = this.soloBefore; this.soloBefore = null; this.soloTrack = null;
      for (let j = 0; j < this.tracks.length; j++) await this.setMute(j, !!b[j]);
      return;
    }
    if (!this.soloBefore) this.soloBefore = this.tracks.map((t) => t.mute);
    this.soloTrack = k;
    for (let j = 0; j < this.tracks.length; j++) await this.setMute(j, j !== k);
  }

  // ------------------------------------------------------------------ transport ---
  async readTransport() {
    const t = await this.ask<{ playing: boolean; counting: boolean; pending: number; rec: number; sel: number }>(req.transport());
    this.playing = t.pending === 1 || t.pending === 3 ? true : t.pending === 2 ? false : t.playing;
    this.counting = t.counting;
    this.recMask = t.rec;
  }
  async transportOp(op: 1 | 2 | 3 | 4) {
    try {
      const t = await this.ask<{ playing: boolean; pending: number; rec: number; counting: boolean }>(req.transport(op));
      this.playing = op === 1 || op === 3 ? true : op === 2 ? false : t.playing || t.pending === 1;
      this.recMask = t.rec;
      this.counting = t.counting;
    } catch (e) { this.fail(e); }
  }
  play() { return this.transportOp(1); }
  stop() { return this.transportOp(2); }
  restart() { return this.transportOp(3); }
  rec() { return this.transportOp(4); }
  togglePlay() { return this.playing ? this.stop() : this.play(); }

  // ------------------------------------------------------------------- projects ---
  async readProj() { this.proj = await this.ask<ProjNames>(req.projName()); }
  /** SAVE AS: the music into slot s with a name (flash: stops the transport) */
  async saveProject(slot: number, name: string): Promise<ProjNames> {
    // (rc2) a lost reply: the same SAVE AS again (it writes the same music: safe), then the unit's word
    const r: ProjNames = await this.writing("project", () => this.ask<ProjNames>(req.projName(1, slot, name), { timeout: 4000, retries: 1 }),
      (): Promise<unknown> => this.saveProject(slot, name));
    this.proj = r;
    if (r.rc) throw new Error(r.rc === 1 ? "refused" : "failed");
    this.interrupted = null;
    return r;
  }
  async renameProject(slot: number, name: string) {
    const r = await this.ask<ProjNames>(req.projName(2, slot, name), { timeout: 4000, retries: 1 });
    this.proj = r;
    if (r.rc) throw new Error(r.rc === 1 ? "refused" : "failed");
  }
  async loadProject(slot: number) {
    const r = await this.ask<{ used: number }>(req.project(0, slot), { timeout: 4000, retries: 1 });
    if (!r.used) throw new Error("empty");
    this.steps = {};
    await this.readTracks(true);
    await this.readEngineDesc(this.tracks[this.sel].engine);
    await this.readProj();
    await this.readTransport();
    if (this.wantSong) await this.readSong(true);
  }

  // ---------------------------------------------------------------------- steps ---
  async loadSteps(k: number) {
    if (!this.client) return;
    this.stepsLoading = k;
    try {
      const n = this.info?.nstep ?? 64, out: Step[] = [];
      for (let i = 0; i < n; i++) out.push(await this.ask<Step>(req.trackStep(k, i)));
      this.steps = { ...this.steps, [k]: out };
    } finally { this.stepsLoading = null; }
  }
  async writeStep(k: number, i: number, s: Step) {
    const arr = this.steps[k];
    if (arr) arr[i] = { ...s };
    try {
      const st = { ...s, chance: s.chance ?? 100, ratchet: s.ratchet ?? 1, cond: s.cond ?? 0, flam: s.flam ?? 0, nudge: s.nudge ?? 0, ties: s.ties ?? 0 };
      const r = await this.ask<Step>(req.trackStep(k, i, st), { key: `s:${k}:${i}` });
      if (this.steps[k]) this.steps[k][i] = r;
      this.touch(k);
    } catch (e) { this.fail(e); }
  }

  // --------------------------------------------------------- motion and locks ---
  motion = $state<Record<number, { on: boolean; events: { step: number; param: number; value: number; lock: boolean }[] }>>({});
  async readMotion(k: number) {
    if (!this.info?.motionMax) return;
    const op = this.info.locks ? 7 : 0;
    const r = await this.ask<{ on: boolean; events: { step: number; param: number; value: number; lock?: boolean }[] }>(req.motion(k, op, {}, this.info.ntrk));
    this.motion = { ...this.motion, [k]: { on: r.on, events: r.events.map((e) => ({ ...e, lock: !!e.lock })) } };
  }
  /** an automation event (op 3) or, with lock, a parameter lock (op 5) at step on parameter id */
  async setMotion(k: number, step: number, id: number, value: number, lock = false) {
    const r = await this.ask<{ rc: number }>(req.motion(k, lock ? 5 : 3, { step, param: id, value }, this.info!.ntrk), { key: `mo:${k}:${step}:${id}` });
    await this.readMotion(k);
    if (r.rc === 2) this.onEvent({ type: "toast", level: "warn", key: "toast.error", vars: { detail: "MOTION FULL (64)" } });
  }
  async delMotion(k: number, step: number, id: number) {
    await this.ask(req.motion(k, 4, { step, param: id }, this.info!.ntrk));
    await this.readMotion(k);
  }
  async motionPlay(k: number, on: boolean) { await this.ask(req.motion(k, 1, { on }, this.info!.ntrk)); await this.readMotion(k); }

  /** several steps of track k at once, one undo for all of them */
  async editSteps(k: number, changes: Record<number, Step>, label = "steps") {
    const cur = this.steps[k];
    if (!cur) return;
    const before: Record<number, Step> = {}, after: Record<number, Step> = {};
    for (const [i, st] of Object.entries(changes)) { before[+i] = { ...cur[+i], notes: [...cur[+i].notes] }; after[+i] = { ...st, notes: [...st.notes] }; }
    this.undoStack.push({ kind: "steps", label, track: k, before, after, at: Date.now() });
    this.redoStack = [];
    for (const [i, st] of Object.entries(after)) cur[+i] = st;          // at once on screen
    for (const [i, st] of Object.entries(after)) await this.writeStep(k, +i, st);
  }

  // ----------------------------------------------------------------------- song ---
  get song2() { return !!(this.info && this.info.song2 & 1); }
  async readSong(withPool = false) {
    const two = this.song2;
    this.lists = await this.ask<Lists>(req.list(0, 0, 0, null, two));
    this.perform = await this.ask<Perform>(req.perform());
    if (withPool || !this.pool) this.pool = await this.ask<Pool>(req.pool());
  }
  /** LIST op (1 SET, 2 COPY, 3 PLAY, 4 CLEAR, 5 REPS, 6 SONG, 7 NEW, 8 COPY+, 9 SOUND) -> rc */
  async listOp(op: number, k = 0, e = 0, v: number | number[] | null = null) {
    const r = await this.ask<Lists>(req.list(op, k, e, v, this.song2), { timeout: 2000, retries: 0 });
    this.lists = r;
    this.pool = await this.ask<Pool>(req.pool());
    return r.rc;
  }
  async poolOp(id: number | null, op: 1 | 2 | 3) {
    const r = await this.ask<Pool>(req.pool(id, op), { timeout: 3000, retries: 0 });
    this.pool = r;
    return r.rc;
  }
  async performOp(op: number, v = 0) {
    const r = await this.ask<Perform>(req.perform(op, v), { timeout: 2000, retries: 0 });
    this.perform = r;
    if (op === 2) await this.readTracks();
    return r.rc;
  }

  // -------------------------------------------------------------- user presets ---
  async readUserSlots() {
    const out: UserSlot[] = [];
    for (let s = 0; s < 32; s += 16) {
      const r = await this.ask<{ slots: UserSlot[] }>(req.upList(s, 16));
      out.push(...r.slots);
    }
    this.userSlots = out;
  }
  async loadUser(k: number, slot: number) {
    if (k !== this.sel) await this.selectTrack(k);
    const r = await this.ask<{ rc: number }>(req.upLoad(slot), { timeout: 2000 });
    if (r.rc) throw new Error("refused");
    const d = await this.ask<{ p: number[]; engine: number; preset: number }>(req.trackDump(k));
    this.tracks[k] = { ...this.tracks[k], engine: d.engine, preset: d.preset, p: d.p, name: this.userSlots[slot]?.name ?? "" };
    await this.readEngineDesc(d.engine);
  }
  async storeUser(slot: number, name: string) {
    const r = await this.writing("user", () => this.ask<{ rc: number }>(req.upStore(slot, name), { timeout: 4000, retries: 1 }),
      () => this.storeUser(slot, name));
    if (r.rc) throw new Error("failed");
    this.interrupted = null;
    await this.readUserSlots();
  }
  async eraseUser(slot: number) {
    const r = await this.ask<{ rc: number }>(req.upErase(slot), { timeout: 4000, retries: 1 });
    if (r.rc) throw new Error("failed");
    await this.readUserSlots();
  }

  // ----------------------------------------------------------------------- menu ---
  async readMenu() {
    const items: MenuItem[] = [];
    for (let i = 0; i < (this.info?.menuCount ?? 0); i++) {
      const d = await this.ask<MenuItem>(req.menuDesc(i));
      if (d.id === 127) break;
      items.push(d);
    }
    this.menu = items;
  }
  async setMenu(id: number, v: number) {
    const it = this.menu.find((m) => m.id === id);
    if (it) it.value = v;
    const r = await this.ask<{ rc: number; value: number }>(req.menuSet(id, v), { key: `m:${id}` });
    if (it) it.value = r.value;
  }

  // ---------------------------------------------------------------------- panel ---
  /** PANEL op 0: the buttons and keys held, as the unit's own contacts (INFO 59 01 bit 1) */
  get hasPanel() { return !!(this.info && this.info.cast & 2); }
  async panel(buttons: number, keys = 0) {
    await this.rq([84, [0, buttons & 127, (buttons >> 7) & 127, keys & 127, (keys >> 7) & 127, (keys >> 14) & 127, (keys >> 21) & 127]]);
  }
  async panelTurn(role: number, steps: number) {
    await this.rq([84, [1, role, steps < 0 ? 128 + Math.max(-63, steps) : Math.min(63, steps)]]);
  }
  /** SEQ TOOLS on track k: hold SEQ (the layer opens after the unit's HOLD time), tap the tool's white key, let go */
  async seqTool(k: number, place: number) {
    if (!this.hasPanel) throw new Error("nopanel");
    if (k !== this.sel) await this.selectTrack(k);
    const seq = 1 << BTN.SEQ, key = 1 << WHITE_BIT[place];
    try {
      await this.panel(seq);
      await sleep(720);                      // MENU › HOLD at most 0.6 s
      await this.panel(seq, key);
      await sleep(90);
      await this.panel(seq);
      await sleep(90);
    } finally { await this.panel(0).catch(() => {}); }
    await sleep(60);
    if (this.steps[k]) await this.loadSteps(k);
    this.touch(k);
  }

  // ------------------------------------------------------------------------ misc ---
  private fail(e: unknown) {
    const m = String((e as Error)?.message || e);
    if (m === "closed") return;
    this.onEvent({ type: "toast", level: "err", key: /timeout/.test(m) ? "toast.noreply" : "toast.error", vars: { detail: m } });
  }
}

export const device = new Device();
