// SPDX-License-Identifier: GPL-3.0-only
import "@fontsource/inter/400.css";
import "@fontsource/inter/500.css";
import "@fontsource/inter/600.css";
import "@fontsource/inter/700.css";
import "./styles/tokens.css";
import "./styles/global.css";
import { mount } from "svelte";
import App from "./App.svelte";

import { device } from "./lib/model/device.svelte";
import { ui } from "./lib/ui.svelte";
import { req } from "./lib/legacy/proto.js";
import * as castApi from "./lib/cast.svelte";

const app = mount(App, { target: document.getElementById("app")! });
// for the end-to-end tests and the curious: the model, and the protocol's request builders (window.__gir.device.ask)
(window as unknown as { __gir: unknown }).__gir = { device, ui, req, cast: castApi };
export default app;
