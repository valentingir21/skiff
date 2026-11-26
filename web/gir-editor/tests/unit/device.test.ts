// SPDX-License-Identifier: GPL-3.0-only
// The model against the virtual FM-1 (the Gir firmware itself): what the editor changes changes in the device, and
// what changes on the device comes back to the editor.
import { afterAll, beforeAll, describe, expect, it } from "vitest";
import { Device, TOOL } from "../../src/lib/model/device.svelte";
import { ID, T, pagesFor, pathOf, ccOf } from "../../src/lib/model/tables";
import { req } from "../../src/lib/legacy/proto.js";
import { NodeDevice } from "./node-device";

let nd: NodeDevice, d: Device;
const eng = (n: string) => T.ENG.findIndex((e) => e.name === n);
beforeAll(async () => {
  nd = new NodeDevice();
  d = new Device();
  d.factory = () => nd;
  nd.start(4);
  await d.connect("emulator");
}, 30000);
afterAll(async () => { await d.disconnect(); });

describe("connect", () => {
  it("loads from the tables, six tracks, live", () => {
    expect(d.status).toBe("ready");
    expect(d.tablesMatch).toBe(true);
    expect(d.tracks.length).toBe(6);
    expect(d.live).toBe(true);
    expect(d.tracks[0].name).not.toBe("");
  });
});

describe("parameters both ways", () => {
  it("editor -> device: a track parameter (TRACK_PARAM for another track)", async () => {
    await d.setParam(2, ID.P_EQ_LOW, 0);
    const r = await d.ask(req.trackParam(2, ID.P_EQ_LOW));
    expect(r.value).toBe(0);
    await d.setParam(0, ID.P_DEPTH, 90);
    expect((await d.ask(req.trackParam(0, ID.P_DEPTH))).value).toBe(90);
  });
  it("device -> editor: a knob turned on the unit arrives (CHANGED push)", async () => {
    const before = [...d.tracks[d.sel].p];
    nd.ex.web_enc(3, 6);                       // KNOB 1 (EN_K1) on HOME: the engine's first HOME parameter
    let id = -1;
    for (let i = 0; i < 50 && id < 0; i++) {
      await new Promise((r) => setTimeout(r, 20));
      id = d.tracks[d.sel].p.findIndex((v, k) => v !== before[k]);
    }
    expect(id).toBeGreaterThanOrEqual(0);
    expect((await d.ask(req.trackParam(d.sel, id))).value).toBe(d.tracks[d.sel].p[id]);
  });
});

describe("Gir engines and pages", () => {
  it("every Gir engine loads on a track", async () => {
    for (const n of ["SUPERSAW", "ACID", "MINI", "HOOVER", "INDUS"]) {
      await d.setEngine(1, eng(n));
      expect(d.engineName(d.tracks[1].engine)).toBe(n);
    }
  });
  it("RUMBLE / RUMBLE 2 / KICK exist on DRUM and DRUM-X only; their knobs reach the device", async () => {
    const titles = (e: number) => pagesFor(e).map((p) => p.title);
    expect(titles(eng("DRUM"))).toEqual(expect.arrayContaining(["RUMBLE", "RUMBLE 2", "KICK", "LANES", "LANE MIX"]));
    expect(titles(eng("DRUM-X"))).toEqual(expect.arrayContaining(["RUMBLE", "RUMBLE 2", "KICK", "SOUND"]));
    expect(titles(eng("ACID"))).not.toContain("RUMBLE");
    expect(titles(eng("MINI"))).toEqual(expect.arrayContaining(["OSC 1", "OSC 2", "OSC 3", "MINI MOD"]));
    await d.setEngine(3, eng("DRUM"));
    for (const [id, v] of [[ID.P_RB_MIX, 100], [ID.P_RB_SLICE, 2], [ID.P_RB_TUNE, 5], [ID.P_RB_ATK, 40], [ID.P_KCLICK, 90], [ID.P_KPUNCH, 70]]) {
      await d.setParam(3, id, v);
      expect((await d.ask(req.trackParam(3, id))).value).toBe(v);
    }
    const rb = pagesFor(eng("DRUM")).find((p) => p.title === "RUMBLE")!;
    expect(pathOf(rb, 1, eng("DRUM"))).toBe("EDIT ×3 › RUMBLE › KNOB 2");
    const eq = T.PAGES.find((p) => p.title === "EQ")!;
    expect(pathOf(eq, 3, 0)).toBe("GLO ×2 › EQ › KNOB 4");
    expect(ccOf(ID.P_DEPTH, 0)).toBe(27);
    expect(ccOf(ID.P_EQ_LOW, 0)).toBe(24);
  });
});

describe("undo", () => {
  it("a knob gesture is one undo; a preset load undoes to the sound before", async () => {
    const before = d.tracks[0].p[ID.P_REV];
    for (const v of [10, 20, 30]) await d.setParam(0, ID.P_REV, v);
    await d.undo();
    expect(d.tracks[0].p[ID.P_REV]).toBe(before);
    expect((await d.ask(req.trackParam(0, ID.P_REV))).value).toBe(before);
    await d.redo();
    expect(d.tracks[0].p[ID.P_REV]).toBe(30);
    const e0 = d.tracks[4].engine, p0 = d.tracks[4].preset;
    await d.loadPreset(4, eng("ACID"), 2);
    expect(d.engineName(d.tracks[4].engine)).toBe("ACID");
    await d.undo();
    expect(d.tracks[4].engine).toBe(e0);
    expect(d.tracks[4].preset).toBe(p0);
  });
});

describe("steps, accent and slide, SEQ TOOLS and the acid generator (PANEL)", () => {
  it("a step with ACC and SLD written and read back", async () => {
    await d.setEngine(5, eng("ACID"));
    await d.loadSteps(5);
    const s = { ...d.steps[5][0], n: 1, notes: [45, 0, 0, 0], time: 0, flags: 3, vel: 100 };
    await d.writeStep(5, 0, s);
    const r = await d.ask(req.trackStep(5, 0));
    expect(r.flags & 3).toBe(3);
    expect(r.notes[0]).toBe(45);
  });
  it("NEW ACID LINE (SEQ held + G4) writes a line on an ACID track", async () => {
    await d.seqTool(5, TOOL.CLEAR);
    expect(d.steps[5].filter((s) => s.time === 0 && s.n > 0).length).toBe(0);
    await d.seqTool(5, TOOL.ACID_NEW);
    const notes = d.steps[5].filter((s) => s.time === 0 && s.n > 0);
    expect(notes.length).toBeGreaterThan(3);
  }, 20000);
});

describe("transport and song", () => {
  it("play / stop", async () => {
    await d.play();
    expect(d.playing).toBe(true);
    await new Promise((r) => setTimeout(r, 300));
    await d.stop();
    expect(d.playing).toBe(false);
  });
  it("lists, COPY+", async () => {
    await d.readSong(true);
    expect(d.lists!.tracks.length).toBe(6);
    const t0 = d.lists!.tracks[0];
    const first = t0.ent.findIndex((e) => e.pat > 0);
    if (first >= 0) {
      const rc = await d.listOp(8, 0, first);
      expect([0, 1]).toContain(rc);
    }
  });
});
