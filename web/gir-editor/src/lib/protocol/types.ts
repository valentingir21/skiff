// SPDX-License-Identifier: GPL-3.0-only
// The shapes proto.js's parsers return (web/EDITOR_PROTOCOL.md), as Gir Editor uses them.

export interface Info {
  version: string;
  nengines: number;
  pcount: number;
  gcount: number;
  nstep: number;
  pe0: number;
  engines: string[];
  ntrk: number;
  chainRows: number;
  uiCaps: number;
  motionMax: number;
  chance: boolean;
  backupCaps: number;
  fm6: { factory: number; bank: number; caps: number } | null;
  syncCaps: number;
  ratchet: number;
  stepx: number;
  locks: boolean;
  menuCount: number;
  kitLanes: number;
  skiff: number;
  song2: number;
  cast: number;
  light: { tracks: number; engines: number };
}

export interface Desc {
  scope: number;
  id: number;
  fmt: number;
  min: number;
  max: number;
  def: number;
  label: string;
  unit: string;
  names: string[];
}

export interface Step {
  index?: number;
  track?: number;
  n: number;
  notes: number[];
  time: number;          // 0 NOTE, 1 TIE, 2 REST
  flags: number;         // 1 accent, 2 slide
  vel: number;
  hit: number;
  acc: number;
  chance: number;
  ratchet: number;
  flam: number;
  cond: number;
  nudge: number;
  ties: number;
}

export interface TrackRow { engine: number; preset: number; level: number; mute: number; armed: number }

export interface ListEntry { pat: number; reps: number; chain: boolean; snd: [number, number] }
export interface ListTrack { cur: number; next: number; pat: number; ent: ListEntry[] }
export interface Lists { op: number; rc: number; song: number; tracks: ListTrack[] }
export interface Pool { op: number; rc: number; pats: { len: number; refs: number }[] }
export interface Perform { op: number; rc: number; launch: number; last: number; scenes: number; tracks: { muted: boolean; staged: number }[] }
export interface Transport { rc: number; playing: boolean; counting: boolean; pending: number; rec: number; sel: number }
export interface ProjNames { op: number; rc: number; cur: number; changed: boolean; name: string; slots: { used: boolean; name: string }[] }
export interface UserSlot { slot: number; used: number; engine: number; name: string }
export interface MenuItem { index: number; id: number; kind: number; value: number; min: number; max: number; name: string; names: string[] | null; unit: string; tab: number; tabName: string }
