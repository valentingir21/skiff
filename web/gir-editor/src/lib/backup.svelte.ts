// SPDX-License-Identifier: GPL-3.0-only
// The full backup and restore (the Projects view, the desktop app's File menu): the original editor's format
// (web/fm1backup.js: the file, its checks, the capture). A restore asks, then always downloads a backup of the FM-1 as
// it is before writing.
// (rc2) The restore is fm1backup.js restoreBackup's, object by object in the same order, with two things a real USB
// link needs: a lost reply no longer ends it (the object is cancelled, op 3, and written again from its start: the unit
// stages an object in RAM and commits it whole, editor_backup.c, so a piece sent twice is refused, never half-taken),
// and an unplugged unit leaves a resumable restore: the objects already committed are remembered, Resume writes the rest.
// Old backups (before Skiff 2.0) go through the original's converter (web/conv) first, with its report.
import * as BK from "../../../fm1backup.js";
import { device } from "./model/device.svelte";
import { ask, download, pickFile, toastKey } from "./ui.svelte";
import { t } from "./i18n/index.svelte";

const { captureBackup, readBackup, bkU32, bkPack } = BK as any;
export const backupState = $state({
  busy: null as null | { what: string; pct: number },
  /** the last restore's converter report (an old backup), shown in the Projects view */
  report: null as null | { file: string; lines: string[] },
});
const stamp = () => new Date().toISOString().slice(0, 10);
/** reads are safe to repeat: a lost reply is asked again (fm1backup.js asks LIST once, GET twice) */
const rqRead = (r: [number, number[]], o: any = {}) => device.rq(r, { ...o, retries: Math.max(o.retries ?? 0, 2) });

export async function backup(prefix = "Skiff-backup"): Promise<boolean> {
  if (backupState.busy || !device.ready) return false;
  backupState.busy = { what: "proj.backingUp", pct: 0 };
  try {
    await device.stop();
    let file: any;
    for (let n = 0; ; n++) {           // (a capture can't be resumed: the unit's snapshot; once more from the start)
      try {
        file = await captureBackup(rqRead, device.info!.version, (d: number, k: number) => (backupState.busy = { what: "proj.backingUp", pct: Math.round((100 * d) / Math.max(1, k)) }));
        break;
      } catch (e) { if (n >= 1 || !device.ready) throw e; }
    }
    const name = `${prefix}-${stamp()}.json`;
    download(name, JSON.stringify(file));
    toastKey("ok", "proj.backupDone", { file: name });
    return true;
  } catch (e) { toastKey("err", "toast.error", { detail: String((e as Error).message) }); return false; }
  finally { backupState.busy = null; }
}

const BANK = (id: number) => id >= 16 && id < 32;
const CHUNK = 256;
class Rc extends Error { constructor(public rc: number) { super(`rc ${rc}`); } }

/** one object, from its start: 0 written, 1 skipped (a firmware without it) */
async function putObject(o: any): Promise<number> {
  const rq = (r: [number, number[]], timeout = 4000) => device.rq(r, { timeout, retries: 0 });
  if (o.id >= 32) {                                   // a sample slot: SMP_BEGIN (erase), the pieces, SMP_END (header)
    const slot = o.id - 32;
    const check = (a: number[]) => { if (a[0] !== slot) throw new Error("Unexpected sample reply"); if (a[a.length - 1]) throw new Rc(a[a.length - 1]); };
    if (!o.size) { check(await rq([14, [slot]])); return 0; }
    check(await rq([11, [slot]]));
    for (let off = 512; off < o.size; off += CHUNK) {
      const chunk = o.bytes.subarray(off, off + CHUNK);   // (a piece again at its offset: the same bytes, idempotent)
      check(await device.rq([12, [slot, off & 127, (off >>> 7) & 127, (off >>> 14) & 127, ...bkPack(chunk)]], { timeout: 1500, retries: 2 }));
    }
    check(await rq([13, [slot, ...bkPack(o.bytes.subarray(0, 480))]]));
    return 0;
  }
  const put = async (args: number[]) => { const a = await rq([67, args]); if (a[2]) throw new Rc(a[2]); return a; };
  const a = await rq([67, [0, o.id, ...bkU32(o.size), ...bkU32(o.crc)]]);
  if (a[2] === 1 && (o.id === 9 || BANK(o.id))) return 1;   // (fm1backup.js: older firmware doesn't take these)
  if (a[2]) throw new Rc(a[2]);
  try {
    for (let off = 0; off < o.size; off += CHUNK) await put([1, o.id, ...bkU32(off), ...bkPack(o.bytes.subarray(off, off + CHUNK))]);
    await put([2, o.id]);
  } catch (e) { await device.rq([67, [3, o.id]], { timeout: 1000, retries: 0 }).catch(() => {}); throw e; }
  return 0;
}

/** the restore itself: objects 2.. (and samples), then 1 (settings), then 0 (the music), as fm1backup.js; `done` the
 *  ids already committed (a resumed restore) */
async function writeArchive(archive: any, done: Set<number>, only: number[] | null = null) {
  const order = [...archive.objects.slice(2), archive.objects[1], archive.objects[0]].filter((o: any) => !only || only.includes(o.id));
  const total = order.reduce((n: number, o: any) => n + o.size, 0);
  let pos = order.filter((o: any) => done.has(o.id)).reduce((n: number, o: any) => n + o.size, 0);
  for (const o of order) {
    if (done.has(o.id)) continue;
    for (let n = 0; ; n++) {
      try { await putObject(o); break; }
      catch (e) {
        // the link went: stop here (resumable). A lost or late reply: this object once more from its start
        if (!device.client || /closed/.test(String((e as Error).message)) || n >= 2) throw e;
        await new Promise((r) => setTimeout(r, 300));
      }
    }
    done.add(o.id);
    pos += o.size;
    backupState.busy = { what: "proj.restoring", pct: Math.round((100 * pos) / Math.max(1, total)) };
  }
}

/** (Skiff 2.0) what the unit can't read as it is (a backup of before 2.0: projects SKF1..3, old patterns, presets
 *  without patches), converted first by the original's converter (web/conv/conv.js + conv.wasm, the firmware's own
 *  code), as the original editor's Restore does on a 2.0 unit -> the archive to write, the lines of its report, the ids
 *  to leave out (damaged: the unit refuses them too); null: nothing to convert */
export async function convertOld(archive: any): Promise<{ archive: any; lines: string[]; only: number[] | null } | null> {
  if (!(device.info && device.info.song2 & 1)) return null;
  // @ts-ignore (plain JS of the original editor)
  const { loadConverter } = await import("../../../conv/conv.js");
  const base = new URL(import.meta.env.BASE_URL || "./", document.baseURI);
  const r = (await loadConverter(new URL("conv/conv.wasm", base).href)).convert(archive);
  if (!r.writeBack.length && !r.report.some((x: any) => x.what !== "kept" || x.note)) return null;
  const bad = r.report.filter((x: any) => x.what.startsWith("refused")).map((x: any) => x.id);
  const name = (id: number) => id === 0 ? t("conv.music") : id <= 5 ? t("conv.slot", { n: "ABCD"[id - 2] })
    : id <= 7 ? t("conv.presets", { n: id === 6 ? "1-16" : "17-32" }) : id === 8 ? t("conv.fm6") : t("conv.patches");
  const lines: string[] = [];
  const pats = r.report.filter((x: any) => x.id >= 16 && x.id < 32 && x.what !== "kept").length;
  for (const x of r.report)
    if ((x.what !== "kept" || x.note) && x.id !== 1 && (x.id < 16 || x.id > 31) && x.id < 32)
      lines.push(`${name(x.id)} : ${x.what.startsWith("refused") ? t("conv.refused") : x.what.startsWith("kept as") ? t("conv.full")
        : x.what === "kept" ? x.was : `${x.was} → ${x.now}`}${x.note ? ` (${t("conv.note")} ${x.note})` : ""}`);
  if (pats) lines.push(t("conv.patterns", { n: pats }));
  return { archive: r.archive, lines, only: bad.length ? r.archive.objects.map((o: any) => o.id).filter((id: number) => !bad.includes(id)) : null };
}

let resumeState: { archive: any; done: Set<number>; name: string; only: number[] | null } | null = null;

export async function restore() {
  if (backupState.busy || !device.ready) return;
  const [f] = await pickFile(".json,application/json");
  if (!f) return;
  let file: any;
  try { file = JSON.parse(await f.text()); readBackup(file); } catch (e) { toastKey("err", "toast.error", { detail: String((e as Error).message) }); return; }
  let archive = readBackup(file), converted: Awaited<ReturnType<typeof convertOld>> = null;
  try { converted = await convertOld(archive); } catch (e) { toastKey("err", "conv.failed", { detail: String((e as Error).message) }); return; }
  const body = t("proj.restoreConfirm", { file: f.name }) + (converted ? "\n\n" + t("conv.intro") + "\n• " + converted.lines.join("\n• ") : "");
  if (!(await ask({ title: t("proj.restore"), body, ok: t("dlg.restore"), danger: true }))) return;
  backupState.report = converted ? { file: f.name, lines: converted.lines } : null;
  if (converted) archive = converted.archive;
  if (!(await backup("Skiff-backup-before-restore"))) return;        // the FM-1 as it is, first
  resumeState = { archive, done: new Set(), name: f.name, only: converted?.only ?? null };
  await runRestore();
}

/** (rc2) after an unplug: the objects not written yet */
export async function resumeRestore() {
  if (!resumeState || backupState.busy || !device.ready) return;
  await runRestore();
}
export const canResume = () => !!resumeState;

async function runRestore() {
  const st = resumeState!;
  backupState.busy = { what: "proj.restoring", pct: 0 };
  try {
    await device.stop();
    await device.writing("restore", () => writeArchive(st.archive, st.done, st.only), () => resumeRestore());
    resumeState = null;
    device.interrupted = null;
    device.steps = {};
    await device.readTracks(true);
    await device.readProj();
    toastKey("ok", "proj.restoreDone");
  } catch (e) {
    const m = String((e as Error).message);
    if (m === "interrupted") toastKey("err", "write.interrupted.restore", { n: st.done.size, total: st.archive.objects.length });
    else toastKey("err", "toast.error", { detail: m });
  }
  finally { backupState.busy = null; }
}

/** (rc2) before installing another firmware with the web installer: it makes no backup itself over a unit of Skiff
 *  2.0 or later (Gir 1.0 is one: index_pkg.html backupBefore), so the editor makes it, then says what comes next */
export const INSTALLER_URL = "https://valentingir21.github.io/skiff/webapp/installer/";
export async function prepareUpdate() {
  if (!device.ready) return;
  if (!(await ask({ title: t("upd.title"), body: t("upd.before"), ok: t("upd.go") }))) return;
  if (!(await backup("Skiff-backup-before-update"))) return;
  const open = await ask({ title: t("upd.title"), body: t("upd.next"), ok: t("upd.open"), cancel: t("dlg.close") });
  if (open) window.open(INSTALLER_URL, "_blank", "noopener");
}
