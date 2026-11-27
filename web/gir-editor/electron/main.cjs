// SPDX-License-Identifier: GPL-3.0-only
// Gir Editor for Mac and Windows: the very web build (dist/) in Electron's Chromium, which has Web MIDI with SysEx
// (macOS's own WebView has no Web MIDI: a Tauri app could not reach the FM-1). Served from app://gir/ (a secure,
// standard scheme: Web MIDI, AudioWorklet and fetch of the virtual FM-1's wasm all work, as on https). MIDI and SysEx
// are granted to this app's own pages only, once and for all. Native menus, always on top, a compact window beside a
// DAW, the window's size and place remembered.
const { app, BrowserWindow, Menu, protocol, net, session, shell, ipcMain, dialog } = require("electron");
const path = require("node:path");
const fs = require("node:fs");
const { pathToFileURL } = require("node:url");

const DIST = path.join(__dirname, "..", "dist");
const ORIGIN = "app://gir";
protocol.registerSchemesAsPrivileged([{ scheme: "app", privileges: { standard: true, secure: true, supportFetchAPI: true, corsEnabled: true, stream: true } }]);

const STATE = () => path.join(app.getPath("userData"), "window.json");
function readState() { try { return JSON.parse(fs.readFileSync(STATE(), "utf8")); } catch { return {}; } }
function writeState(s) { try { fs.writeFileSync(STATE(), JSON.stringify(s)); } catch { /* read-only */ } }

let win = null;
let state = {};

// One Gir Editor talks to the FM-1 at a time: this app and the plugin's instances (plugin/src/PortLock.cpp) share
// ~/Library/Application Support/Gir Editor/port-lock.json {id, pid, app, label, t}. Its owner rewrites t every second;
// older than 4 s, or its process gone, it is free. Taking over rewrites it; the old owner hears it at its next beat.
const LOCK = () => path.join(process.env.GIR_LOCK_DIR || path.join(app.getPath("appData"), "Gir Editor"), "port-lock.json");
const LOCK_ID = require("node:crypto").randomUUID();
let lockBeat = null;
function readLock() { try { return JSON.parse(fs.readFileSync(LOCK(), "utf8")); } catch { return null; } }
function alive(pid) { try { process.kill(pid, 0); return true; } catch (e) { return e.code === "EPERM"; } }
function writeLock() {
  try {
    fs.mkdirSync(path.dirname(LOCK()), { recursive: true });
    const tmp = LOCK() + "." + process.pid;
    fs.writeFileSync(tmp, JSON.stringify({ id: LOCK_ID, pid: process.pid, app: "app", label: "app", t: Date.now() }));
    fs.renameSync(tmp, LOCK());
  } catch { /* read-only: no lock */ }
}
function lockAcquire(force) {
  const h = readLock();
  if (h && h.id !== LOCK_ID && Date.now() - h.t <= 4000 && alive(h.pid) && !force) return { ok: false, by: h.label || h.app || "" };
  writeLock();
  clearInterval(lockBeat);
  lockBeat = setInterval(() => {
    const now = readLock();
    if (now && now.id !== LOCK_ID) {
      clearInterval(lockBeat); lockBeat = null;
      if (win && !win.isDestroyed()) win.webContents.send("gir", "taken", now.label || now.app || "");
      return;
    }
    writeLock();
  }, 1000);
  return { ok: true };
}
function lockRelease() {
  if (!lockBeat) return;
  clearInterval(lockBeat); lockBeat = null;
  const h = readLock();
  if (h && h.id === LOCK_ID) { try { fs.unlinkSync(LOCK()); } catch { /* gone */ } }
}

function create() {
  state = readState();
  const compact = !!state.compact;
  const b = (compact ? state.compactBounds : state.bounds) || {};
  win = new BrowserWindow({
    width: b.width || (compact ? 560 : 1440), height: b.height || (compact ? 460 : 900),
    x: b.x, y: b.y, minWidth: compact ? 420 : 1024, minHeight: compact ? 320 : 680,
    backgroundColor: "#09090a", title: "Gir Editor", show: false,
    titleBarStyle: process.platform === "darwin" ? "hiddenInset" : "default",
    trafficLightPosition: { x: 14, y: 18 },
    alwaysOnTop: !!state.onTop,
    icon: path.join(__dirname, "..", "build", "icon.png"),
    webPreferences: { preload: path.join(__dirname, "preload.cjs"), contextIsolation: true, sandbox: true, nodeIntegration: false },
  });
  if (state.maximized && !compact) win.maximize();
  if (state.onTop) win.setAlwaysOnTop(true, "floating");   // (remembered, at the level the menu sets)
  win.once("ready-to-show", () => win.show());
  win.loadURL(`${ORIGIN}/index.html${compact ? "?compact=1" : ""}`);
  const save = () => {
    if (!win || win.isDestroyed()) return;
    const k = state.compact ? "compactBounds" : "bounds";
    if (!win.isMaximized() && !win.isFullScreen()) state[k] = win.getBounds();
    state.maximized = win.isMaximized();
    writeState(state);
  };
  win.on("resize", save); win.on("move", save); win.on("close", save);
  win.webContents.setWindowOpenHandler(({ url }) => { if (/^https?:/.test(url)) shell.openExternal(url); return { action: "deny" }; });
  win.webContents.on("will-navigate", (e, url) => { if (!url.startsWith(ORIGIN)) { e.preventDefault(); if (/^https?:/.test(url)) shell.openExternal(url); } });
}

function setCompact(on) {
  if (!win) return;
  const k0 = state.compact ? "compactBounds" : "bounds";
  state[k0] = win.getBounds();
  state.compact = on;
  writeState(state);
  const b = (on ? state.compactBounds : state.bounds) || {};
  win.setMinimumSize(on ? 420 : 1024, on ? 320 : 680);
  win.setBounds({ x: b.x ?? win.getBounds().x, y: b.y ?? win.getBounds().y, width: b.width || (on ? 560 : 1440), height: b.height || (on ? 460 : 900) });
  win.loadURL(`${ORIGIN}/index.html${on ? "?compact=1" : ""}`);
  menu();
}

const send = (what, arg) => win && win.webContents.send("gir", what, arg);
function menu() {
  const mac = process.platform === "darwin";
  const views = ["Sound", "Sequencer", "Song", "Mixer", "Library", "Projects", "Samples", "FM6", "Screen", "Settings"];
  const tpl = [
    ...(mac ? [{ role: "appMenu" }] : []),
    { label: "File", submenu: [
      { label: "Back Up the FM-1…", accelerator: "CmdOrCtrl+Shift+B", click: () => send("backup") },
      { label: "Restore a Backup…", click: () => send("restore") },
      { label: "Prepare a Firmware Update…", click: () => send("update") },
      { type: "separator" },
      { label: "Save the Music to the FM-1", accelerator: "CmdOrCtrl+S", click: () => send("save") },
      { type: "separator" },
      mac ? { role: "close" } : { role: "quit" },
    ] },
    { label: "Edit", submenu: [
      { label: "Undo", accelerator: "CmdOrCtrl+Z", click: () => send("undo") },
      { label: "Redo", accelerator: "Shift+CmdOrCtrl+Z", click: () => send("redo") },
      { type: "separator" }, { role: "cut" }, { role: "copy" }, { role: "paste" }, { role: "selectAll" },
      { type: "separator" },
      { label: "Command Palette…", accelerator: "CmdOrCtrl+K", click: () => send("palette") },
    ] },
    { label: "View", submenu: [
      ...views.map((v, i) => ({ label: v, accelerator: i < 9 ? `Alt+${i + 1}` : undefined, click: () => send("view", v.toLowerCase()) })),
      { type: "separator" }, { role: "togglefullscreen" }, { role: "resetZoom" }, { role: "zoomIn" }, { role: "zoomOut" },
      ...(app.isPackaged ? [] : [{ role: "toggleDevTools" }]),
    ] },
    { label: "Window", submenu: [
      { label: "Always on Top", type: "checkbox", checked: !!state.onTop, click: (m) => { state.onTop = m.checked; win.setAlwaysOnTop(m.checked, "floating"); writeState(state); } },
      { label: "Compact Window (beside a DAW)", type: "checkbox", checked: !!state.compact, accelerator: "CmdOrCtrl+Shift+M", click: (m) => setCompact(m.checked) },
      { type: "separator" }, { role: "minimize" }, ...(mac ? [{ role: "zoom" }, { role: "front" }] : []),
    ] },
    { role: "help", submenu: [
      { label: "Gir on GitHub", click: () => shell.openExternal("https://github.com/valentingir21/skiff") },
      { label: "Try Gir in the Browser", click: () => shell.openExternal("https://valentingir21.github.io/skiff/try/") },
      { label: "Keyboard Shortcuts", accelerator: "?", click: () => send("help") },
    ] },
  ];
  Menu.setApplicationMenu(Menu.buildFromTemplate(tpl));
}

app.whenReady().then(() => {
  // the app's own pages from dist/, nothing else
  protocol.handle("app", (req) => {
    const u = new URL(req.url);
    const file = path.normalize(path.join(DIST, decodeURIComponent(u.pathname)));
    if (!file.startsWith(DIST)) return new Response("forbidden", { status: 403 });
    return net.fetch(pathToFileURL(file).toString());
  });
  // MIDI (with SysEx: the FM-1's editor protocol) and audio for this app's pages: granted without asking each launch
  const ok = (wc, perm) => (perm === "midi" || perm === "midiSysex") && (!wc || wc.getURL().startsWith(ORIGIN));
  session.defaultSession.setPermissionRequestHandler((wc, perm, cb) => cb(ok(wc, perm)));
  session.defaultSession.setPermissionCheckHandler((wc, perm) => ok(wc, perm));
  ipcMain.handle("gir:compact", (_e, on) => setCompact(!!on));
  ipcMain.handle("gir:lock", (_e, force) => lockAcquire(!!force));
  ipcMain.handle("gir:unlock", () => lockRelease());
  ipcMain.handle("gir:ontop", (_e, on) => { state.onTop = !!on; win.setAlwaysOnTop(!!on, "floating"); writeState(state); menu(); });
  ipcMain.handle("gir:state", () => ({ compact: !!state.compact, onTop: !!state.onTop, platform: process.platform, version: app.getVersion() }));
  create();
  menu();
  app.on("activate", () => { if (BrowserWindow.getAllWindows().length === 0) create(); });
});
app.on("window-all-closed", () => { if (process.platform !== "darwin") app.quit(); });
void dialog;

app.on("will-quit", () => lockRelease());
