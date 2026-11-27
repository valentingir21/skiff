// SPDX-License-Identifier: GPL-3.0-only
// (plugin, desktop app) where the Gir Editor holding the FM-1 is, from the lock's label (plugin/src/PortLock.cpp):
// "host|track|instance" for a plugin ("Ableton Live 12|Bass|2"), "app" for the desktop app
import { t } from "../i18n/index.svelte";

export function whereText(label: string): string {
  if (!label || label === "held" || label === "taken") return t("plugin.whereUnknown");
  if (label === "app") return t("plugin.whereApp");
  const [host, track, n] = label.split("|");
  const on = track ? t("plugin.whereTrack", { name: track }) : t("plugin.whereInstance", { n: n || "?" });
  return host ? `${host}, ${on}` : on;
}
