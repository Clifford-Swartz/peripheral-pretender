// Peripheral Pretender: the Data Visualizer panel. Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
//
// What Data Visualizer shows from a kit's debugger, from the twin's: the
// server is the DGI host (dgiview.py) and sends the interfaces this kit
// offers ("dgiinfo"), the bytes they deliver ("dgidata") and the GPIO
// edges with the debugger's timestamps ("dgigpio"). Byte streams are
// decoded as Data Stream packets (0x03, fields, 0xFC) with the fields the
// format box names, and plotted; GPIO lines are drawn as a timing view.
"use strict";

const DV = (() => {
  const WINDOW_S = 10;                  // the graph's time span
  const GPIO_WINDOW_US = 50000;         // the GPIO view's span
  const COLORS = ["#ff6b5e", "#5ec8ff", "#7dff6b", "#ffd21a", "#d08bff", "#ff9f40"];
  const FIELD = { u8: [1, "getUint8"], i8: [1, "getInt8"], u16: [2, "getUint16"],
                  i16: [2, "getInt16"], u32: [4, "getUint32"], i32: [4, "getInt32"],
                  f32: [4, "getFloat32"] };
  let ifaces = [], gpioNames = [], rest = {}, series = [], raw = "";
  let gpio = { edges: [], now: 0 }, enabled = new Set();

  const $ = (id) => document.getElementById(id);

  function fields() {
    return $("dv-format").value.split(/[ ,]+/).filter((f) => f in FIELD);
  }

  // Data Stream packets in @bytes (with what was left over before):
  // each one's fields, little-endian as the firmware sends them
  function decode(id, bytes) {
    let buf = new Uint8Array([...(rest[id] || []), ...bytes]);
    const fs = fields(), size = fs.reduce((n, f) => n + FIELD[f][0], 0);
    const out = [];
    let i = 0;
    for (;;) {
      const s = buf.indexOf(0x03, i);
      if (s < 0) { buf = new Uint8Array(0); break; }
      const e = buf.indexOf(0xfc, s + 1 + size);
      if (e < 0) { buf = buf.slice(s); break; }
      if (e === s + 1 + size) {
        const dv = new DataView(buf.buffer, buf.byteOffset + s + 1, size);
        let o = 0;
        out.push(fs.map((f) => { const v = dv[FIELD[f][1]](o, true); o += FIELD[f][0]; return v; }));
      }
      i = e + 1;
    }
    rest[id] = buf.length > 4096 ? buf.slice(-4096) : buf;
    return out;
  }

  function info(msg) {
    ifaces = msg.interfaces;
    $("dv-head").hidden = $("dv").hidden = false;
    $("dv-name").textContent = `${msg.name} ${msg.version}`;
    const box = $("dv-ifaces");
    box.textContent = "";
    for (const f of ifaces) {
      const lab = document.createElement("label");
      lab.innerHTML = `<input type="checkbox" value="${f.id}"> ${f.name}`;
      const cb = lab.querySelector("input");
      cb.checked = enabled.has(f.id);
      cb.addEventListener("change", () => {
        if (cb.checked) enabled.add(f.id); else enabled.delete(f.id);
        send({ type: "dgi", enable: [...enabled] });
      });
      box.appendChild(lab);
    }
    send({ type: "dgi", enable: [...enabled] });
  }

  function data(msg) {
    const bytes = Uint8Array.from(atob(msg.data), (c) => c.charCodeAt(0));
    const now = performance.now() / 1000;
    const name = (ifaces.find((f) => f.id === msg.iface) || {}).name || msg.iface;
    raw = (raw + `${name}: ` + [...bytes].map((b) => b.toString(16).padStart(2, "0")).join(" ") + "\n").slice(-3000);
    $("dv-raw").textContent = raw;
    $("dv-raw").scrollTop = $("dv-raw").scrollHeight;
    for (const vals of decode(msg.iface, bytes)) series.push([now, vals]);
    const cut = now - WINDOW_S;
    while (series.length && series[0][0] < cut) series.shift();
  }

  function gpioEdges(msg) {
    for (const [t, lv] of msg.edges) gpio.edges.push([t, lv]);
    if (gpio.edges.length) gpio.now = gpio.edges[gpio.edges.length - 1][0];
    // Keep the level before the window, so lanes start at the right level
    const cut = gpio.now - GPIO_WINDOW_US;
    while (gpio.edges.length > 1 && gpio.edges[1][0] < cut) gpio.edges.shift();
  }

  function drawGraph() {
    const cv = $("dv-graph"), ctx = cv.getContext("2d");
    const w = cv.width, h = cv.height;
    ctx.fillStyle = "#0b0d10"; ctx.fillRect(0, 0, w, h);
    if (!series.length) {
      ctx.fillStyle = "#55606e"; ctx.font = "11px system-ui";
      ctx.fillText("Data Stream packets (0x03 … 0xFC) plot here", 10, h / 2);
      return;
    }
    const now = performance.now() / 1000, n = series[0][1].length;
    let lo = Infinity, hi = -Infinity;
    for (const [, v] of series) for (const x of v) { lo = Math.min(lo, x); hi = Math.max(hi, x); }
    if (hi === lo) { hi += 1; lo -= 1; }
    const y = (v) => h - 12 - (v - lo) / (hi - lo) * (h - 24);
    for (let k = 0; k < n; k++) {
      ctx.strokeStyle = COLORS[k % COLORS.length];
      ctx.beginPath();
      series.forEach(([t, v], i) => {
        const x = w - (now - t) / WINDOW_S * w;
        if (i) ctx.lineTo(x, y(v[k])); else ctx.moveTo(x, y(v[k]));
      });
      ctx.stroke();
    }
    ctx.fillStyle = "#8a94a3"; ctx.font = "10px system-ui";
    ctx.fillText(String(+hi.toFixed(3)), 4, 10);
    ctx.fillText(String(+lo.toFixed(3)), 4, h - 2);
    const last = series[series.length - 1][1];
    ctx.fillText(last.map((v, k) => `${fields()[k]}: ${+v.toFixed(3)}`).join("   "), 60, 10);
  }

  // GPIO lanes: the last GPIO_WINDOW_US of debugger time, each line's
  // level as a trace, with the period of its last two rising edges
  function drawGpio() {
    const cv = $("dv-gpio"), ctx = cv.getContext("2d");
    const w = cv.width, h = cv.height, lines = gpioNames.length || 4;
    const lane = h / lines;
    ctx.fillStyle = "#0b0d10"; ctx.fillRect(0, 0, w, h);
    ctx.font = "10px system-ui";
    const x0 = 70, span = w - x0;
    const tx = (t) => x0 + (t - (gpio.now - GPIO_WINDOW_US)) / GPIO_WINDOW_US * span;
    for (let l = 0; l < lines; l++) {
      const top = l * lane + 4, bot = (l + 1) * lane - 4;
      ctx.fillStyle = "#8a94a3";
      ctx.fillText(gpioNames[l] || `GPIO${l}`, 4, (top + bot) / 2 + 3);
      if (!gpio.edges.length) continue;
      ctx.strokeStyle = COLORS[l % COLORS.length];
      ctx.beginPath();
      let prev = null;
      const rises = [];
      for (const [t, lv] of gpio.edges) {
        const v = (lv >> l) & 1, x = Math.max(x0, tx(t)), yy = v ? top : bot;
        if (prev === null) ctx.moveTo(x, yy);
        else { ctx.lineTo(x, prev ? top : bot); ctx.lineTo(x, yy); }
        if (prev === 0 && v === 1) rises.push(t);
        prev = v;
      }
      ctx.lineTo(w, prev ? top : bot);
      ctx.stroke();
      if (rises.length >= 2) {
        const p = rises[rises.length - 1] - rises[rises.length - 2];
        ctx.fillStyle = "#c9d1db";
        ctx.fillText(p >= 1000 ? `${(p / 1000).toFixed(3)} ms` : `${p.toFixed(1)} µs`, w - 60, top + 9);
      }
    }
    ctx.fillStyle = "#55606e";
    ctx.fillText(`${GPIO_WINDOW_US / 1000} ms`, w - 34, h - 2);
  }

  function frame() {
    if (!$("dv").hidden) { drawGraph(); drawGpio(); }
    requestAnimationFrame(frame);
  }
  requestAnimationFrame(frame);

  return {
    board(b) {
      gpioNames = (b.dgi && b.dgi.gpio) || [];
      $("dv-head").hidden = $("dv").hidden = true;
      series = []; raw = ""; rest = {}; gpio = { edges: [], now: 0 };
    },
    reset() { series = []; raw = ""; rest = {}; gpio = { edges: [], now: 0 }; $("dv-raw").textContent = ""; },
    info, data, gpio: gpioEdges,
  };
})();
