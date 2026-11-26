// SPDX-License-Identifier: GPL-3.0-only
// The generated tables (tools/gen_editor_tables.c) say what the device itself says: every DESC of every track
// parameter, on a track of each engine, and every engine's EDIT descriptors and preset names. If this fails, the
// firmware changed: run tools/gen_editor_tables.sh.
import { afterAll, beforeAll, describe, expect, it } from "vitest";
import tables from "../../src/generated/tables.json";
import { Client, parseNamesTolerant } from "../../src/lib/protocol/client";
import { CMD, req, parse } from "../../src/lib/legacy/proto.js";
import { NodeDevice } from "./node-device";

let dev: NodeDevice, c: Client;
beforeAll(async () => {
  dev = new NodeDevice(); await dev.connect(); dev.start();
  c = new Client(dev, { onPush: () => {} });
  c.info = await c.ask(req.info());
});
afterAll(async () => { c.close(); await dev.disconnect(); });

const strip = (d: any) => ({ label: d.label, fmt: d.fmt, min: d.min, max: d.max, def: d.def, unit: d.unit, names: d.fmt === 8 ? d.names : [] });
const T = tables as any;

describe("generated tables match the firmware", () => {
  it("INFO", () => {
    expect(c.info!.pcount).toBe(T.P_COUNT);
    expect(c.info!.pe0).toBe(T.P_E0);
    expect(c.info!.gcount).toBe(T.G_COUNT);
    expect(c.info!.ntrk).toBe(T.NTRK);
    expect(c.info!.engines).toEqual(T.ENG.map((e: any) => e.name));
  });
  it("track and global descriptors (track 1 on each engine)", async () => {
    for (const e of T.ORDER as number[]) {
      await c.ask(req.set(1, T.ID.G_ENGSEL, e));
      for (let id = 0; id < T.P_E0; id++) {
        const d = await c.ask(req.desc(0, id));
        const t = T.TP[id];
        expect(strip(d), `engine ${T.ENG[e].name} id ${id}`).toEqual(strip({ ...t, names: t.names || [] }));
      }
      for (let k = 0; k < 8; k++) {
        const d = await c.ask(req.desc(0, T.P_E0 + k));
        const t = T.ENG[e].edit[k];
        // (SAMPLE's SET names follow the user samples; a fresh unit has none: the table's)
        expect(strip(d), `engine ${T.ENG[e].name} E${k + 1}`).toEqual(strip({ ...t, names: t.names || [] }));
      }
      // (Gir 1.0: a reply stops at 600 bytes, editor.c ed_out: ANALOG's 89 names come cut; what comes is the table's)
      const n = parseNamesTolerant(await c.rq(req.names(e)));
      const all = T.ENG[e].presets.map((p: any) => p.name.slice(0, 12));   // (ed_str: 12 at most)
      expect(n.count).toBe(all.length);
      expect(n.names).toEqual(all.slice(0, n.names.length));
      if (T.ENG[e].name !== "ANALOG") { expect(n.complete).toBe(true); expect(n.titles).toEqual(T.ENG[e].titles); }
      else expect(n.complete).toBe(false);
    }
    for (let id = 0; id < T.G_COUNT; id++) {
      const d = await c.ask(req.desc(1, id));
      const t = T.GP[id];
      expect(strip(d), `global ${id}`).toEqual(strip({ ...t, names: t.names || [] }));
    }
  }, 60000);
});
