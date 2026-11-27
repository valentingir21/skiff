// SPDX-License-Identifier: GPL-3.0-only
// The desktop app's bridge: the menus' commands to the page, and the window's compact / always-on-top switches
const { contextBridge, ipcRenderer } = require("electron");
contextBridge.exposeInMainWorld("girDesktop", {
  on: (cb) => ipcRenderer.on("gir", (_e, what, arg) => cb(what, arg)),
  compact: (on) => ipcRenderer.invoke("gir:compact", on),
  onTop: (on) => ipcRenderer.invoke("gir:ontop", on),
  state: () => ipcRenderer.invoke("gir:state"),
  lock: (force) => ipcRenderer.invoke("gir:lock", force),
  unlock: () => ipcRenderer.invoke("gir:unlock"),
});
