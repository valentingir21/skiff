// SPDX-License-Identifier: GPL-3.0-only
// The interface's own state: the view shown, the notifications, the confirmation dialog, the overlays.
import { prefs } from "./store";
import { t } from "./i18n/index.svelte";

export type View = "sound" | "seq" | "song" | "mixer" | "library" | "screen" | "fm6" | "samples" | "projects" | "settings" | "demo";
export const VIEWS: View[] = ["sound", "seq", "song", "mixer", "library", "projects", "samples", "fm6", "screen", "settings"];

export interface Toast { id: number; level: "info" | "ok" | "warn" | "err"; text: string; action?: { label: string; run: () => void } }
export interface Confirm { title: string; body: string; ok: string; cancel?: string; danger?: boolean; input?: { label: string; value: string; max?: number }; resolve: (v: string | boolean | null) => void }

let tid = 0;
export const ui = $state({
  view: prefs.get<View>("view", "sound"),
  toasts: [] as Toast[],
  confirm: null as Confirm | null,
  palette: false,
  help: false,
  presetsOpen: true,
  progress: null as { text: string; pct: number } | null,
  /** what the pointer is over: shown in the help bar (role, range, default, CC, where on the FM-1) */
  hint: null as null | { label: string; text: string; range: string; def: string; cc: number | null; where: string; color: string },
});

export function go(v: View) { ui.view = v; prefs.set("view", v); }

export function toast(level: Toast["level"], text: string, action?: Toast["action"], ms = level === "err" ? 7000 : 3800) {
  const id = ++tid;
  ui.toasts = [...ui.toasts.slice(-3), { id, level, text, action }];
  setTimeout(() => { ui.toasts = ui.toasts.filter((x) => x.id !== id); }, ms);
}
export function toastKey(level: Toast["level"], key: string, vars?: Record<string, string | number>) { toast(level, t(key, vars)); }

/** a confirmation (or a name to type): resolves true / the text, or null when cancelled */
export function ask(c: Omit<Confirm, "resolve">): Promise<string | boolean | null> {
  return new Promise((resolve) => { ui.confirm = { ...c, resolve: (v) => { ui.confirm = null; resolve(v); } }; });
}

export function download(name: string, data: string | Uint8Array, type = "application/json") {
  const blob = new Blob([data as BlobPart], { type });
  const a = document.createElement("a");
  a.href = URL.createObjectURL(blob);
  a.download = name;
  document.body.appendChild(a);
  a.click();
  a.remove();
  setTimeout(() => URL.revokeObjectURL(a.href), 2000);
}

export function pickFile(accept: string, multiple = false): Promise<File[]> {
  return new Promise((res) => {
    const i = document.createElement("input");
    i.type = "file"; i.accept = accept; i.multiple = multiple;
    i.onchange = () => res([...(i.files || [])]);
    i.click();
  });
}

export const TRACK_COLORS = ["var(--t1)", "var(--t2)", "var(--t3)", "var(--t4)", "var(--t5)", "var(--t6)"];
