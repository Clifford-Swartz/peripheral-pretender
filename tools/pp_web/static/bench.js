// Peripheral Pretender parts bench. Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
//
// Parts on the board itself: an SVG layer over the board picture, in the
// stage's millimetres. Parts sit on or beside the board and jumper wires
// run from their legs into the holes of the board's headers (board.json
// "pin_headers", sent as "bench_holes"), to pad tags or to rail tags. The
// server solves the circuit (bench.py) and sends "benchstate"; this draws
// it: LED glow, motor spin, servo horn, meter readings, wire voltages.
//
// Editing: drag parts and tags; click a part's leg, then a header hole
// (or another leg or tag) to run a wire; select a part or wire and press
// Delete, or right-click it. While it runs: hold a button, drag a pot's
// knob, double-click a switch.
"use strict";

const Bench = (() => {
  const SVGNS = "http://www.w3.org/2000/svg";
  let lib = {}, pins = [], holes = {}, data = { parts: [], wires: [], anchors: {} };
  let solved = { parts: {}, pads: {}, nets: {}, warnings: [] };
  let svg, layerHoles, layerWires, layerParts, rubber;
  let wiring = null, selected = null, drag = null, held = false;
  let stage = { x0: 0, y0: 0, w: 100, h: 100 };   // the board's view, mm
  const spin = {};                       // motor id -> angle

  // Terminal offsets and the drawing of each part type, at (0, 0), in
  // drawing units (one screen pixel each: see unit())
  const SHAPES = {
    resistor: { terms: { a: [-40, 0], b: [40, 0] }, draw: (p) =>
      `<path class="lead" d="M-40 0H-24M24 0H40"/>
       <rect class="body" x="-24" y="-8" width="48" height="16" rx="4"/>
       <path class="band" d="M-16 -8V8M-9 -8V8M-2 -8V8M14 -8V8"/>
       <text y="-13">${fmtOhms(p.ohms)}</text>` },
    led: { terms: { a: [-32, 0], k: [32, 0] }, draw: (p) =>
      `<circle class="glow" r="22" fill="${LED_COLORS[p.color] || "#f33"}"/>
       <path class="lead" d="M-32 0H-10M10 0H32"/>
       <path class="body" fill="${LED_COLORS[p.color] || "#f33"}" d="M-10 -10V10L10 0Z"/>
       <path class="lead" d="M10 -10V10"/>
       <text y="-16">${p.color} LED</text>` },
    diode: { terms: { a: [-32, 0], k: [32, 0] }, draw: () =>
      `<path class="lead" d="M-32 0H-10M10 0H32"/>
       <path class="body" d="M-10 -10V10L10 0Z"/><path class="lead" d="M10 -10V10"/>
       <text y="-16">diode</text>` },
    rgb_led: { terms: { r: [-36, -16], g: [-36, 0], b: [-36, 16], k: [36, 0] },
      draw: () =>
      `<circle class="glow" r="26" fill="#fff"/>
       <path class="lead" d="M-36 -16H-16M-36 0H-16M-36 16H-16M16 0H36"/>
       <circle class="body rgb" r="16"/>
       <text y="-30">RGB LED</text>` },
    button: { terms: { a: [-32, 0], b: [32, 0] }, draw: () =>
      `<path class="lead" d="M-32 0H-16M16 0H32"/>
       <rect class="body cap" x="-16" y="-14" width="32" height="28" rx="6"/>
       <circle class="knob" r="8"/>
       <text y="-19">button (hold)</text>` },
    switch: { terms: { a: [-32, 0], b: [32, 0] }, draw: (p) =>
      `<path class="lead" d="M-32 0H-14M14 0H32"/>
       <circle class="dot" cx="-14" r="3"/><circle class="dot" cx="14" r="3"/>
       <path class="lever ${p.on ? "on" : ""}" d="${p.on ? "M-14 0L14 0" : "M-14 0L10 -14"}"/>
       <rect class="hit" x="-18" y="-18" width="36" height="24"/>
       <text y="-21">switch: ${p.on ? "on" : "off"}</text>` },
    pot: { terms: { a: [-40, 0], b: [40, 0], w: [0, 34] }, draw: (p) =>
      `<path class="lead" d="M-40 0H-24M24 0H40M0 34V14"/>
       <rect class="body" x="-24" y="-8" width="48" height="16" rx="4"/>
       <rect class="track" x="-22" y="-3" width="44" height="6" rx="3"/>
       <circle class="knob pot-knob" cx="${-20 + 40 * p.position}" r="7"/>
       <text y="-13">${fmtOhms(p.ohms)} pot</text>` },
    motor: { terms: { a: [-38, 0], b: [38, 0] }, draw: () =>
      `<path class="lead" d="M-38 0H-22M22 0H38"/>
       <circle class="body" r="22"/>
       <g class="rotor"><path d="M0 -17V17M-17 0H17" class="blade"/><circle r="4" class="hub"/></g>
       <text y="-27">motor</text>` },
    nmos: { terms: { g: [-36, 0], d: [16, -32], s: [16, 32] }, draw: () =>
      `<rect class="hitbox" x="-30" y="-26" width="50" height="52"/>
       <path class="lead" d="M-36 0H-8M-8 -12V12M-2 -14V14M-2 -9H16V-32M-2 9H16V32M-2 0H16"/>
       <path class="lead" d="M4 -4L-2 0L4 4"/>
       <text x="22" y="4" text-anchor="start">N-FET</text>` },
    servo: { terms: { sig: [-44, -12], vcc: [-44, 0], gnd: [-44, 12] }, draw: () =>
      `<path class="lead" d="M-44 -12H-26M-44 0H-26M-44 12H-26"/>
       <rect class="body" x="-26" y="-18" width="52" height="36" rx="4"/>
       <g class="horn"><path d="M0 0V-30" class="arm"/><circle r="6" class="hub"/></g>
       <text y="32">servo</text>` },
    voltmeter: { terms: { p: [-24, 22], n: [24, 22] }, draw: () =>
      `<path class="lead" d="M-24 22V12M24 22V12"/>
       <rect class="body meter" x="-32" y="-14" width="64" height="26" rx="4"/>
       <text class="reading" y="4">-- V</text>
       <text class="pm" x="-24" y="34">+</text><text class="pm" x="24" y="34">−</text>` },
  };
  const TAG_TERM = [34, 0];
  const LED_COLORS = { red: "#ff3b2f", yellow: "#ffd21a", orange: "#ff8c1a",
                       green: "#2bff55", blue: "#3b82ff", white: "#ffffff" };
  const RAIL_COLORS = { "3V3": "#ff6b5e", "5V": "#ffae42", GND: "#6f7c8c" };
  const WIRE_COLORS = ["#e8322d", "#2d7fe8", "#2bbf55", "#f0b400", "#b05cff", "#ff7a1a"];

  function fmtOhms(r) {
    return r >= 1e6 ? `${+(r / 1e6).toFixed(2)} MΩ` : r >= 1e3 ? `${+(r / 1e3).toFixed(2)} kΩ`
      : `${+r.toFixed(1)} Ω`;
  }
  function fmtV(v) { return v == null ? "" : `${v.toFixed(2)} V`; }
  function el(tag, attrs = {}, html) {
    const e = document.createElementNS(SVGNS, tag);
    for (const [k, v] of Object.entries(attrs)) e.setAttribute(k, v);
    if (html != null) e.innerHTML = html;
    return e;
  }

  // Millimetres per drawing unit: parts keep their size on screen
  function unit() {
    const w = svg && svg.getBoundingClientRect().width;
    return w ? stage.w / w : 0.2;
  }
  // Board coordinates (mm, y up) to the stage's (mm from top-left)
  function fromBoard([x, y]) { return [x - stage.x0, stage.y0 - y]; }

  function endPos(end) {
    if (end.startsWith("hole:")) return holes[end] ? holes[end].at : null;
    const u = unit();
    if (end.startsWith("pin:") || end.startsWith("rail:")) {
      const at = data.anchors[end];
      return at ? [at[0] + TAG_TERM[0] * u, at[1] + TAG_TERM[1] * u] : null;
    }
    const [pid, term] = end.split(".");
    const p = data.parts.find((x) => x.id === pid);
    const off = p && SHAPES[p.type] && SHAPES[p.type].terms[term];
    return off ? [p.x + off[0] * u, p.y + off[1] * u] : null;
  }

  function netVolts(end) {
    if (end.startsWith("hole:") && holes[end]) end = holes[end].net;
    return solved.nets[end];
  }
  function endName(end) {
    const h = holes[end];
    if (!h) return end;
    const net = h.net.split(":")[1];
    return `${end.slice(5)} ${h.label}${h.label === net ? "" : ` (${net})`}`;
  }

  function commit() { send({ type: "bench", bench: data }); }

  function nextId(type) {
    const stem = { resistor: "r", led: "led", rgb_led: "rgb", diode: "d", button: "sw",
                   switch: "s", pot: "pot", motor: "m", nmos: "q", servo: "servo",
                   voltmeter: "vm" }[type] || type;
    let n = 1;
    while (data.parts.some((p) => p.id === `${stem}${n}`)) n++;
    return `${stem}${n}`;
  }

  // A free spot for a new part: the stage's free area beside the board
  // (or below it), stepping across then down
  function freeSpot() {
    const u = unit(), dx = 140 * u, dy = 90 * u;
    const taken = [...data.parts.map((p) => [p.x, p.y]), ...Object.values(data.anchors)];
    // Beside the board when the stage has room there, else below it,
    // starting under the headers so the wires stay short
    const hx = Object.values(holes).map((h) => h.at[0]);
    const area = board && board.size_mm && stage.w - board.size_mm[0] > 30
      ? [board.origin_mm[0] - stage.x0 + board.size_mm[0] + 8, 10]
      : [hx.length ? Math.max(5, Math.min(...hx) - 30 * u) : 10,
         (board ? board.size_mm[1] : 0) + 12 * u];
    for (let y = area[1] + dy / 2; y < stage.h - dy / 3; y += dy) {
      for (let x = area[0] + dx / 2; x < stage.w - dx / 3; x += dx) {
        if (!taken.some(([a, b]) => Math.abs(a - x) < dx * 0.8 && Math.abs(b - y) < dy * 0.8)) return [x, y];
      }
    }
    return [area[0] + dx / 2, area[1] + dy / 2];
  }

  function addPart(type) {
    const [x, y] = freeSpot();
    const part = { id: nextId(type), type, x, y, ...lib[type].props };
    data.parts.push(part);
    selected = { part: part.id };
    commit();
    render();
  }

  function addAnchor(end) {
    if (data.anchors[end]) return;
    data.anchors[end] = freeSpot();
    commit();
    render();
  }

  function removeSelected() {
    if (!selected) return;
    if (selected.wire != null) data.wires.splice(selected.wire, 1);
    if (selected.part) {
      const id = selected.part;
      data.parts = data.parts.filter((p) => p.id !== id);
      data.wires = data.wires.filter((w) => !w.some((e) => e.startsWith(`${id}.`)));
    }
    if (selected.anchor) {
      const end = selected.anchor;
      delete data.anchors[end];
      data.wires = data.wires.filter((w) => !w.includes(end));
    }
    selected = null;
    commit();
    render();
  }

  function toStage(e) {
    const r = svg.getBoundingClientRect();
    return [(e.clientX - r.left) / r.width * stage.w, (e.clientY - r.top) / r.height * stage.h];
  }

  function clickEnd(end, e) {
    e.stopPropagation();
    e.preventDefault();
    if (wiring && wiring !== end) {
      const pair = [wiring, end];
      if (!data.wires.some(([a, b]) => (a === pair[0] && b === pair[1]) || (a === pair[1] && b === pair[0]))) {
        data.wires.push(pair);
        commit();
      }
      wiring = null;
      render();
      return;
    }
    wiring = wiring === end ? null : end;
    render();
  }

  function terminal(g, end, x, y) {
    const t = el("circle", { class: "term", cx: x, cy: y, r: 5 });
    t.dataset.end = end;
    if (wiring === end) t.classList.add("armed");
    t.appendChild(el("title"));
    t.addEventListener("pointerdown", (e) => clickEnd(end, e));
    g.appendChild(t);
  }

  // Drag a part or a tag by pointer; the window follows the pointer so
  // the layer itself can let clicks through to the board
  function draggable(g, getPos, setPos) {
    g.addEventListener("pointerdown", (e) => {
      if (e.button !== 0 || e.target.classList.contains("term")) return;
      e.stopPropagation();
      const [x0, y0] = toStage(e), [px, py] = getPos();
      drag = { dx: px - x0, dy: py - y0, moved: false, setPos };
    });
  }

  function renderHoles() {
    const u = unit();
    const used = new Set(data.wires.flat());
    for (const [end, h] of Object.entries(holes)) {
      if (!h.net) continue;
      const c = el("circle", { class: "hole", cx: h.at[0], cy: h.at[1], r: Math.max(0.75, 5 * u) });
      c.dataset.end = end;
      if (used.has(end)) c.classList.add("used");
      if (wiring === end) c.classList.add("armed");
      const title = el("title");
      title.textContent = endName(end);
      c.appendChild(title);
      c.addEventListener("pointerdown", (e) => clickEnd(end, e));
      layerHoles.appendChild(c);
    }
  }

  function renderAnchors() {
    const u = unit();
    for (const [end, at] of Object.entries(data.anchors)) {
      const g = el("g", { class: "tag", transform: `translate(${at[0]},${at[1]}) scale(${u})` });
      g.dataset.end = end;
      const isPin = end.startsWith("pin:"), name = end.slice(isPin ? 4 : 5);
      const info = isPin ? pins.find((p) => p.pad === name) : null;
      const label = isPin ? `${name}${info && info.label ? ` ${info.label}` : ""}` : name;
      g.innerHTML = `<rect x="-36" y="-15" width="70" height="30" rx="5"/>
        <text x="-1" y="-2" class="tag-name">${label}</text>
        <text x="-1" y="10" class="tag-sub"></text>`;
      if (selected && selected.anchor === end) g.classList.add("selected");
      draggable(g, () => data.anchors[end], (x, y) => { data.anchors[end] = [x, y]; });
      g.addEventListener("click", (e) => {
        if (drag && drag.moved) return;
        e.stopPropagation(); selected = { anchor: end }; render();
      });
      g.addEventListener("contextmenu", (e) => {
        e.preventDefault(); selected = { anchor: end }; removeSelected();
      });
      const title = el("title");
      title.textContent = `${label} (right-click to remove)`;
      g.appendChild(title);
      terminal(g, end, ...TAG_TERM);
      layerParts.appendChild(g);
    }
  }

  function renderParts() {
    const u = unit();
    for (const p of data.parts) {
      const shape = SHAPES[p.type];
      if (!shape) continue;
      const g = el("g", { class: `part bp-${p.type}`, transform: `translate(${p.x},${p.y}) scale(${u})` },
                   shape.draw(p));
      g.dataset.id = p.id;
      if (selected && selected.part === p.id) g.classList.add("selected");
      const title = el("title");
      title.textContent = `${p.id}: ${lib[p.type].name}`;
      g.prepend(title);
      draggable(g, () => [p.x, p.y], (x, y) => { p.x = x; p.y = y; });
      g.addEventListener("click", (e) => {
        if (drag && drag.moved) return;
        e.stopPropagation(); selected = { part: p.id }; render();
      });
      g.addEventListener("contextmenu", (e) => {
        e.preventDefault(); selected = { part: p.id }; removeSelected();
      });
      partControls(g, p);
      for (const [term, [x, y]] of Object.entries(shape.terms)) terminal(g, `${p.id}.${term}`, x, y);
      layerParts.appendChild(g);
    }
  }

  // The parts you work while the circuit runs
  function partControls(g, p) {
    if (p.type === "button") {
      const cap = g.querySelector(".cap");
      const press = (down) => {
        if (!!p.pressed === down) return;
        p.pressed = down;
        held = down;
        g.classList.toggle("down", down);
        send({ type: "bench-prop", id: p.id, prop: "pressed", value: down });
      };
      cap.addEventListener("pointerdown", (e) => {
        e.stopPropagation();
        try { cap.setPointerCapture(e.pointerId); } catch (_) { /* synthetic */ }
        press(true);
      });
      cap.addEventListener("pointerup", () => press(false));
      cap.addEventListener("pointercancel", () => press(false));
      g.classList.toggle("down", !!p.pressed);
    }
    if (p.type === "switch") {
      g.querySelector(".hit").addEventListener("dblclick", (e) => {
        e.stopPropagation();
        send({ type: "bench-prop", id: p.id, prop: "on", value: !p.on });
      });
    }
    if (p.type === "pot") {
      const knob = g.querySelector(".pot-knob");
      let last = 0;
      knob.addEventListener("pointerdown", (e) => {
        e.stopPropagation();
        try { knob.setPointerCapture(e.pointerId); } catch (_) { /* synthetic */ }
        knob.dataset.held = "1";
        held = true;
      });
      knob.addEventListener("pointermove", (e) => {
        if (!knob.dataset.held) return;
        const [x] = toStage(e);
        const pos = Math.max(0, Math.min(1, ((x - p.x) / unit() + 20) / 40));
        p.position = pos;
        knob.setAttribute("cx", -20 + 40 * pos);
        const now = performance.now();
        if (now - last > 40) {
          last = now;
          send({ type: "bench-prop", id: p.id, prop: "position", value: pos });
        }
      });
      const up = () => {
        if (!knob.dataset.held) return;
        delete knob.dataset.held;
        held = false;
        send({ type: "bench-prop", id: p.id, prop: "position", value: p.position });
      };
      knob.addEventListener("pointerup", up);
      knob.addEventListener("pointercancel", up);
    }
  }

  // Jumper wires: a sagging curve between the ends, coloured per wire,
  // with a thin core showing the voltage
  function renderWires() {
    const u = unit();
    data.wires.forEach(([a, b], i) => {
      const pa = endPos(a), pb = endPos(b);
      if (!pa || !pb) return;
      const len = Math.hypot(pb[0] - pa[0], pb[1] - pa[1]);
      const mx = (pa[0] + pb[0]) / 2, my = (pa[1] + pb[1]) / 2 + Math.min(len * 0.25, 60 * u);
      const d = `M${pa[0]} ${pa[1]}Q${mx} ${my} ${pb[0]} ${pb[1]}`;
      const g = el("g", { class: "wire" });
      g.appendChild(el("path", { d, class: "wire-hit" }));
      g.appendChild(el("path", { d, class: "wire-line", stroke: WIRE_COLORS[i % WIRE_COLORS.length] }));
      const core = el("path", { d, class: "wire-core" });
      core.dataset.end = a;
      g.appendChild(core);
      for (const [x, y] of [pa, pb]) g.appendChild(el("circle", { class: "wire-end", cx: x, cy: y, r: 3.5 * u }));
      if (selected && selected.wire === i) g.classList.add("selected");
      const title = el("title");
      title.dataset.ends = `${endName(a)} — ${endName(b)}`;
      title.dataset.end = a;
      g.appendChild(title);
      g.addEventListener("pointerdown", (e) => e.stopPropagation());
      g.addEventListener("click", (e) => { e.stopPropagation(); selected = { wire: i }; render(); });
      g.addEventListener("contextmenu", (e) => {
        e.preventDefault(); selected = { wire: i }; removeSelected();
      });
      layerWires.appendChild(g);
    });
  }

  function voltColor(v) {
    if (v == null) return "#55606e";
    const t = Math.max(0, Math.min(1, v / 5));
    return `hsl(${210 - 210 * t}, ${40 + 50 * t}%, ${45 + 15 * t}%)`;
  }

  // The solved state on the drawn parts, without rebuilding them
  function paint() {
    if (!layerParts) return;
    for (const g of layerParts.querySelectorAll(".part")) {
      const p = data.parts.find((x) => x.id === g.dataset.id);
      const st = solved.parts[g.dataset.id] || {};
      if (!p) continue;
      if (p.type === "led" || p.type === "diode") {
        const glow = g.querySelector(".glow");
        if (glow) glow.style.opacity = st.brightness ? Math.min(1, 0.15 + Math.sqrt(st.brightness)) : 0;
      }
      if (p.type === "rgb_led") {
        const [r, gg, b] = st.rgb || [0, 0, 0];
        const m = Math.max(r, gg, b), k = m ? 1 / m : 0;
        const col = `rgb(${255 * r * k},${255 * gg * k},${255 * b * k})`;
        g.querySelector(".rgb").setAttribute("fill", m ? col : "#2a2f36");
        const glow = g.querySelector(".glow");
        glow.setAttribute("fill", col);
        glow.style.opacity = m ? Math.min(1, 0.15 + Math.sqrt(m)) : 0;
      }
      if (p.type === "voltmeter") {
        g.querySelector(".reading").textContent = st.volts != null ? fmtV(st.volts) : "-- V";
      }
      if (p.type === "servo") {
        const horn = g.querySelector(".horn");
        horn.setAttribute("transform", `rotate(${st.angle != null ? st.angle - 90 : 0})`);
        horn.classList.toggle("idle", st.angle == null);
      }
      if (p.type === "motor") {
        g.dataset.rpm = st.rpm || 0;
        g.querySelector("text").textContent = st.rpm ? `motor ${st.rpm} rpm` : "motor";
      }
      g.classList.toggle("warn", !!st.warn);
      g.querySelector("title").textContent = `${p.id}: ${lib[p.type].name}${st.warn ? ` — ${st.warn}` : ""}`;
    }
    for (const g of layerParts.querySelectorAll(".tag")) {
      const end = g.dataset.end, isPin = end.startsWith("pin:");
      const name = end.slice(isPin ? 4 : 5), pad = isPin ? solved.pads[name] : null;
      let fill = RAIL_COLORS[name] || "#26303a", sub = "";
      if (pad) {
        const v = pad.volts;
        fill = pad.driven ? (v > 1.65 ? "#7a2620" : "#1d3b66") : "#26303a";
        sub = pad.driven ? `out ${fmtV(v)}` : pad.reads != null ? `in ${fmtV(v)} → ${pad.reads}` : fmtV(v);
        if (pad.amps != null) sub = `out ${fmtV(v)} · ${(pad.amps * 1000).toFixed(1)} mA`;
      } else if (!isPin) {
        sub = name === "GND" ? "0 V" : name === "5V" ? "5 V" : "VDD";
      } else sub = "not wired";
      const rect = g.querySelector("rect");
      rect.setAttribute("fill", fill);
      rect.classList.toggle("warn", !!(pad && pad.warn));
      g.querySelector(".tag-sub").textContent = sub;
    }
    for (const core of layerWires.querySelectorAll(".wire-core")) {
      core.setAttribute("stroke", voltColor(netVolts(core.dataset.end)));
    }
    for (const t of layerWires.querySelectorAll("title")) {
      const v = netVolts(t.dataset.end);
      t.textContent = `${t.dataset.ends}${v != null ? ` · ${fmtV(v)}` : ""} (right-click to remove)`;
    }
    for (const t of svg.querySelectorAll(".term, .hole")) {
      const v = netVolts(t.dataset.end), title = t.querySelector("title");
      if (title) title.textContent = `${endName(t.dataset.end)}${v != null ? ` · ${fmtV(v)}` : ""}`;
    }
    for (const h of layerHoles.querySelectorAll(".hole")) {
      const net = holes[h.dataset.end].net;
      const pad = net.startsWith("pin:") && solved.pads[net.slice(4)];
      h.classList.toggle("high", !!pad && pad.volts > 1.65);
    }
    const warn = document.getElementById("bench-warn");
    if (warn) warn.textContent = (solved.warnings || []).join(" · ");
  }

  function render() {
    if (!svg) return;
    svg.classList.toggle("wiring", !!wiring);
    for (const l of [layerHoles, layerWires, layerParts]) l.textContent = "";
    renderHoles();
    renderWires();
    renderAnchors();
    renderParts();
    paint();
    showProps();
  }

  // The selected part's properties, editable in the bar
  function showProps() {
    const box = document.getElementById("bench-props");
    box.textContent = "";
    const p = selected && selected.part && data.parts.find((x) => x.id === selected.part);
    if (!p) {
      box.innerHTML = wiring
        ? `<span class="muted">wire from <b>${endName(wiring)}</b>: click a header hole, a leg or a tag (Esc cancels)</span>`
        : `<span class="muted">click a part's leg, then a hole on the board's headers, to wire it; right-click removes</span>`;
      return;
    }
    const head = document.createElement("b");
    head.textContent = p.id;
    box.appendChild(head);
    for (const [k, def] of Object.entries(lib[p.type].props)) {
      if (k === "pressed") continue;
      const lab = document.createElement("label");
      lab.textContent = `${k} `;
      let input;
      if (k === "color") {
        input = document.createElement("select");
        input.innerHTML = Object.keys(LED_COLORS).map((c) => `<option>${c}</option>`).join("");
        input.value = p[k];
      } else if (typeof def === "boolean") {
        input = document.createElement("input");
        input.type = "checkbox";
        input.checked = !!p[k];
      } else {
        input = document.createElement("input");
        input.type = "number";
        input.step = "any";
        input.value = p[k];
        input.style.width = "7em";
      }
      input.addEventListener("change", () => {
        const v = input.type === "checkbox" ? input.checked
          : input.type === "number" ? Number(input.value) : input.value;
        send({ type: "bench-prop", id: p.id, prop: k, value: v });
      });
      lab.appendChild(input);
      box.appendChild(lab);
    }
    const del = document.createElement("button");
    del.textContent = "Remove";
    del.addEventListener("click", removeSelected);
    box.appendChild(del);
  }

  function animate(t) {
    for (const g of layerParts ? layerParts.querySelectorAll(".part.bp-motor") : []) {
      const id = g.dataset.id, rpm = Number(g.dataset.rpm || 0);
      spin[id] = ((spin[id] || 0) + rpm * 6 * (t - (animate.last || t)) / 1000) % 360;
      const rotor = g.querySelector(".rotor");
      if (rotor) rotor.setAttribute("transform", `rotate(${spin[id]})`);
    }
    animate.last = t;
    requestAnimationFrame(animate);
  }

  function buildBar() {
    const add = document.getElementById("bench-add");
    add.innerHTML = `<option value="">+ part…</option>` +
      Object.entries(lib).map(([t, d]) => `<option value="${t}">${d.name}</option>`).join("");
    add.onchange = () => { if (add.value) addPart(add.value); add.value = ""; };
    const pin = document.getElementById("bench-pin");
    pin.innerHTML = `<option value="">+ pin tag…</option>` +
      `<optgroup label="Rails">${["3V3", "5V", "GND"].map((r) => `<option value="rail:${r}">${r}</option>`).join("")}</optgroup>` +
      `<optgroup label="Pads">${pins.map((p) => `<option value="pin:${p.pad}">${p.pad}${p.label ? ` (${p.label})` : ""}${p.adc ? " · ADC" : ""}</option>`).join("")}</optgroup>`;
    pin.onchange = () => { if (pin.value) addAnchor(pin.value); pin.value = ""; };
    document.getElementById("bench-clear").onclick = () => {
      if (!data.parts.length && !Object.keys(data.anchors).length) return;
      if (!confirm("Remove every part and wire?")) return;
      data = { parts: [], wires: [], anchors: {} };
      selected = wiring = null;
      commit();
      render();
    };
  }

  function init() {
    svg = document.getElementById("bench-svg");
    layerHoles = el("g");
    layerWires = el("g");
    layerParts = el("g");
    rubber = el("path", { class: "rubber" });
    svg.append(layerWires, layerParts, layerHoles, rubber);
    window.addEventListener("pointermove", (e) => {
      if (!svg) return;
      const [x, y] = toStage(e);
      if (drag) {
        drag.moved = true;
        drag.setPos(x + drag.dx, y + drag.dy);
        render();
      }
      const p = wiring && endPos(wiring);
      rubber.setAttribute("d", p ? `M${p[0]} ${p[1]}L${x} ${y}` : "");
    });
    window.addEventListener("pointerup", () => {
      if (drag && drag.moved) commit();
      setTimeout(() => { drag = null; }, 0);
    });
    // A click on the board (not on a part, hole or wire) ends wiring and
    // drops the selection
    document.getElementById("board").addEventListener("click", () => {
      if (wiring || selected) { wiring = null; selected = null; render(); }
    });
    document.addEventListener("keydown", (e) => {
      if (e.target.closest && e.target.closest("input, textarea, select")) return;
      if (e.key === "Escape") { wiring = null; selected = null; render(); }
      if ((e.key === "Delete" || e.key === "Backspace") && selected) {
        e.preventDefault(); removeSelected();
      }
    });
    new ResizeObserver(() => render()).observe(svg);
    requestAnimationFrame(animate);
  }

  let board = null;
  return {
    init,
    board(msg) {
      board = msg.board;
      lib = msg.bench_lib || {};
      pins = msg.bench_pins || [];
      const v = board.view_mm || { origin: board.origin_mm, size: board.size_mm };
      stage = { x0: v.origin[0], y0: v.origin[1], w: v.size[0], h: v.size[1] };
      svg.setAttribute("viewBox", `0 0 ${stage.w} ${stage.h}`);
      holes = {};
      for (const [end, h] of Object.entries(msg.bench_holes || {})) {
        holes[end] = { ...h, at: fromBoard(h.at_mm) };
      }
      document.getElementById("bench-pane").hidden = !Object.keys(lib).length;
      buildBar();
      render();
    },
    bench(b) {
      // Keep a part being dragged where the pointer has it, and a held
      // button or knob under the pointer
      if (drag || held) return;
      data = { parts: b.parts || [], wires: b.wires || [], anchors: b.anchors || {} };
      for (const end of data.wires.flat()) {
        if (!data.anchors[end] && /^(pin|rail):/.test(end)) data.anchors[end] = freeSpot();
      }
      if (selected && selected.part && !data.parts.some((p) => p.id === selected.part)) selected = null;
      render();
    },
    state(s) {
      solved = s;
      paint();
    },
  };
})();
