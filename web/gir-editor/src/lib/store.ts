// SPDX-License-Identifier: GPL-3.0-only
// A tiny key-value store on IndexedDB (falls back to memory): the demo FM-1's flash, the library of sounds, the
// editor's favourites and preferences. Nothing leaves this computer.
const DB = "gir-editor", OS = "kv";
let dbp: Promise<IDBDatabase | null> | null = null;
const mem = new Map<string, unknown>();

function db(): Promise<IDBDatabase | null> {
  if (dbp) return dbp;
  dbp = new Promise((res) => {
    try {
      if (typeof indexedDB === "undefined") return res(null);
      const r = indexedDB.open(DB, 1);
      r.onupgradeneeded = () => r.result.createObjectStore(OS);
      r.onsuccess = () => res(r.result);
      r.onerror = () => res(null);
    } catch { res(null); }
  });
  return dbp;
}

export async function kvGet<T>(key: string): Promise<T | undefined> {
  const d = await db();
  if (!d) return mem.get(key) as T | undefined;
  return new Promise((res) => {
    const r = d.transaction(OS).objectStore(OS).get(key);
    r.onsuccess = () => res(r.result as T | undefined);
    r.onerror = () => res(undefined);
  });
}

export async function kvSet(key: string, value: unknown): Promise<void> {
  const d = await db();
  if (!d) { mem.set(key, value); return; }
  return new Promise((res) => {
    const t = d.transaction(OS, "readwrite");
    t.objectStore(OS).put(value, key);
    t.oncomplete = () => res();
    t.onerror = () => res();
  });
}

export async function kvDel(key: string): Promise<void> {
  const d = await db();
  if (!d) { mem.delete(key); return; }
  return new Promise((res) => {
    const t = d.transaction(OS, "readwrite");
    t.objectStore(OS).delete(key);
    t.oncomplete = () => res();
    t.onerror = () => res();
  });
}

/** small synchronous preferences (language, favourites, panel sizes) */
export const prefs = {
  get<T>(k: string, def: T): T {
    try { const v = localStorage.getItem("gir." + k); return v == null ? def : (JSON.parse(v) as T); } catch { return def; }
  },
  set(k: string, v: unknown) { try { localStorage.setItem("gir." + k, JSON.stringify(v)); } catch { /* private mode */ } },
};
