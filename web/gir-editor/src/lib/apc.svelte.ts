// SPDX-License-Identifier: GPL-3.0-only
// (rc2) The Akai APC mini mk2 as the original editor's Song tab uses it (web/editor.html: apcPorts, apcAction, apcLeds,
// the same mapping): its 8 x 8 pads are the song, six columns for T1..T6's entries (A..H, page 2: I..P), the last two
// the 16 scenes; the row under the pads mutes T1..T6; its two page buttons; PLAY; the six first faders are T1..T6's
// levels (CC 7 on the tracks' channels). Its lights show the song as the unit plays it: green the entry playing (the
// next one blinking), the track's colour for an entry with a pattern (dimmer for a REST), white the stored scenes.
// It needs only Web MIDI without SysEx (its own port, apart from the FM-1's).
import { APC, apcPorts, apcAction, apcLeds, apcNear } from "./legacy/proto.js";
import { device } from "./model/device.svelte";

export type ApcStatus = "off" | "nomidi" | "denied" | "absent" | "on";
export const apc = $state({
  status: "off" as ApcStatus,
  name: "",
  page: 0,
  /** what its lights show: note -> [status byte, colour velocity], for the editor's picture of it */
  leds: [] as [number, number, number][],
  last: "" as string,
});
interface Port { send(d: number[]): void }
let access: MIDIAccess | null = null;
let input: MIDIInput | null = null;
let output: Port | null = null;
let shown = new Map<number, number>();
let timer: ReturnType<typeof setInterval> | null = null;

export async function apcStart() {
  if (apc.status === "on") return;
  if (typeof navigator === "undefined" || !navigator.requestMIDIAccess) { apc.status = "nomidi"; return; }
  try { access = access ?? (await navigator.requestMIDIAccess({ sysex: false })); }
  catch { apc.status = "denied"; return; }
  access.onstatechange = () => attach();
  attach();
}
function attach() {
  const p = access && (apcPorts(access) as [MIDIInput, MIDIOutput] | null);
  if (input && (!p || p[0].id !== input.id)) { input.onmidimessage = null; input = null; output = null; stopLoop(); }
  if (p && !input) {
    [input, output] = p;
    input.onmidimessage = (e) => { if (e.data) press(Array.from(e.data)); };
    input.open?.().catch(() => {});
    use(p[0].name || "APC mini mk2");
  }
  if (!input && !output) apc.status = "absent";
}
/** (tests) a controller made of two functions, as the real one's ports */
export function apcFake(send: (d: number[]) => void) { output = { send }; use("APC mini mk2 (test)"); }
function use(name: string) {
  apc.status = "on"; apc.name = name; apc.page = 0; shown = new Map();
  device.setSongWatch(true);
  stopLoop();
  timer = setInterval(show, 120);
  show();
}
function stopLoop() { if (timer) { clearInterval(timer); timer = null; device.setSongWatch(false); } }

const playing = () => device.playing;
export function show() {
  const css = getComputedStyle(document.documentElement);
  const tracks = [1, 2, 3, 4, 5, 6].map((k) => apcNear(css.getPropertyValue(`--t${k}`)));
  const m: Map<number, [number, number]> = apcLeds(device.lists, device.perform, apc.page, playing(), tracks);
  const out: [number, number, number][] = [];
  for (const [n, [st, v]] of m) {
    out.push([n, st, v]);
    if (!output || shown.get(n) === ((st << 8) | v)) continue;
    try { output.send([st, n, v]); shown.set(n, (st << 8) | v); } catch { /* unplugged: statechange says so */ }
  }
  apc.leds = out;
}
/** a message from it (a pad, a button, a fader) */
export async function press(data: number[]) {
  const a = apcAction(data, apc.page);
  if (!a || !device.ready) return;
  const caps = (device.info as any)?.skiff ?? 0;
  try {
    if (a.do === "page") { apc.page = a.page; show(); }
    else if (a.do === "entry" && caps & 8) { await device.listOp(3, a.k, a.e); apc.last = `T${a.k + 1} ${"ABCDEFGHIJKLMNOP"[a.e]}`; }
    else if (a.do === "scene" && caps & 16) { await device.performOp(2, a.s); apc.last = `SCENE ${a.s + 1}`; }
    else if (a.do === "mute" && caps & 16) { await device.performOp(3, a.k); apc.last = `MUTE T${a.k + 1}`; }
    else if (a.do === "play" && caps & 1) { await device.togglePlay(); apc.last = "PLAY"; }
    else if (a.do === "level") { device.sendMidi([0xb0 | a.k, 7, a.v]); if (device.tracks[a.k]) device.tracks[a.k].level = Math.round((a.v * 127) / 127); apc.last = `LEVEL T${a.k + 1}`; }
  } catch { /* the next poll shows the unit as it is */ }
  show();
}
export function apcStop() {
  stopLoop();
  try { if (output) for (const n of apcLeds(null, null, 0, false).keys()) output.send([APC.BTN, n, 0]); } catch { /* gone */ }
  if (input) input.onmidimessage = null;
  input = null; output = null; apc.status = "off";
}
if (typeof window !== "undefined") window.addEventListener("pagehide", () => apcStop());
export { APC };
