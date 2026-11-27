// SPDX-License-Identifier: GPL-3.0-only
// In the desktop app (Electron, electron/preload.cjs): its native menus' commands, its compact window
import { device } from "./model/device.svelte";
import { ui, go, type View } from "./ui.svelte";
import { backup, restore, prepareUpdate } from "./backup.svelte";

interface Desktop { on(cb: (what: string, arg?: string) => void): void; compact(on: boolean): Promise<void>; onTop(on: boolean): Promise<void>; state(): Promise<{ compact: boolean; onTop: boolean; platform: string; version: string }>;
  /** (the port lock shared with the plugin, electron/main.cjs) */ lock?(force: boolean): Promise<{ ok: boolean; by?: string }>; unlock?(): Promise<void> }
export const desktop: Desktop | null = (globalThis as unknown as { girDesktop?: Desktop }).girDesktop ?? null;

export function initDesktop() {
  if (!desktop) return;
  document.documentElement.classList.add("desktop", "platform-" + (navigator.platform.toLowerCase().includes("mac") ? "mac" : "other"));
  desktop.on((what, arg) => {
    if (what === "view" && arg) go(arg === "sequencer" ? "seq" : (arg as View));
    else if (what === "undo") device.undo();
    else if (what === "redo") device.redo();
    else if (what === "palette") ui.palette = !ui.palette;
    else if (what === "help") ui.help = !ui.help;
    else if (what === "backup") backup();
    else if (what === "restore") restore();
    else if (what === "update") prepareUpdate();
    else if (what === "taken") (device.transport as { taken?: (by: string) => void } | null)?.taken?.(arg ?? "");
    else if (what === "save") (document.querySelector(".top .save") as HTMLButtonElement | null)?.click();
  });
}
