// SPDX-License-Identifier: GPL-3.0-only
// The full backup and restore (the Projects view, the desktop app's File menu): the original editor's format and
// code (web/fm1backup.js). A restore asks, then always downloads a backup of the FM-1 as it is before writing.
import * as BK from "../../../fm1backup.js";
import { device } from "./model/device.svelte";
import { ask, download, pickFile, toastKey } from "./ui.svelte";
import { t } from "./i18n/index.svelte";

const { captureBackup, restoreBackup, readBackup } = BK as any;
export const backupState = $state({ busy: null as null | { what: string; pct: number } });
const rq = (r: [number, number[]], o?: any) => device.rq(r, o);
const stamp = () => new Date().toISOString().slice(0, 10);

export async function backup(prefix = "Skiff-backup"): Promise<boolean> {
  if (backupState.busy || !device.ready) return false;
  backupState.busy = { what: "proj.backingUp", pct: 0 };
  try {
    await device.stop();
    const file = await captureBackup(rq, device.info!.version, (d: number, n: number) => (backupState.busy = { what: "proj.backingUp", pct: Math.round((100 * d) / Math.max(1, n)) }));
    const name = `${prefix}-${stamp()}.json`;
    download(name, JSON.stringify(file));
    toastKey("ok", "proj.backupDone", { file: name });
    return true;
  } catch (e) { toastKey("err", "toast.error", { detail: String((e as Error).message) }); return false; }
  finally { backupState.busy = null; }
}

export async function restore() {
  if (backupState.busy || !device.ready) return;
  const [f] = await pickFile(".json,application/json");
  if (!f) return;
  let file: any;
  try { file = JSON.parse(await f.text()); readBackup(file); } catch (e) { toastKey("err", "toast.error", { detail: String((e as Error).message) }); return; }
  if (!(await ask({ title: t("proj.restore"), body: t("proj.restoreConfirm", { file: f.name }), ok: t("dlg.restore"), danger: true }))) return;
  if (!(await backup("Skiff-backup-before-restore"))) return;        // the FM-1 as it is, first
  backupState.busy = { what: "proj.restoring", pct: 0 };
  try {
    await restoreBackup(rq, file, (d: number, n: number) => (backupState.busy = { what: "proj.restoring", pct: Math.round((100 * d) / Math.max(1, n)) }));
    device.steps = {};
    await device.readTracks(true);
    await device.readProj();
    toastKey("ok", "proj.restoreDone");
  } catch (e) { toastKey("err", "toast.error", { detail: String((e as Error).message) }); }
  finally { backupState.busy = null; }
}
