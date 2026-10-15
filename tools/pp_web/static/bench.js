// Peripheral Pretender parts bench. Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
//
// Parts on an SVG bench below the board, wired to the board's pads and
// rails. The server solves the circuit (bench.py) and sends "benchstate";
// this draws it: LED glow, motor spin, servo horn, meter readings, net
// voltages. Editing: drag parts and tags, click a terminal then another
// to wire them, select a part or wire and press Delete (or right-click
// it), hold a button, drag a pot's knob, double-click a switch.
"use strict";

const Bench = (() => {
  const SVGNS = "http://www.w3.org/2000/svg";
  let lib = {}, pins = [], data = { parts: [], wires: [], anchors: {} };
  let solved = { parts: {}, pads: {}, nets: {}, warnings: [] };
  let svg, layerWires, layerParts, rubber;
  let wiring = null, selected = null, drag = null;
  const spin = {};                       // motor id -> angle
  let held = false;                      // a button or knob is held

  // Terminal offsets and the drawing of each part type, at (0, 0)
  const SHAPES = {
    resistor: { terms: { a: [-40, 0], b: [40, 0] }, draw: (p) =>
      `<path class="lead" d="M-40 0H-24M24 0H40"/>
       <rect class="body" x="-24" y="-8" width="48" height="16" rx="4"/>
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
      `<path class="lead" d="M-40 0H-24M24 0H40M0 34V${14}"/>
       <rect class="body" x="-24" y="-8" width="48" height="16" rx="4"/>
       <path class="wiper" d="M${-20 + 40 * p.position} 14L${-24 + 40 * p.position} 22H${-16 + 40 * p.position}Z"/>
       <rect class="track" x="-22" y="-3" width="44" height="6" rx="3"/>
       <circle class="knob pot-knob" cx="${-20 + 40 * p.position}" r="7"/>
       <text y="-13">${fmtOhms(p.ohms)} pot</text>` },
    motor: { terms: { a: [-38, 0], b: [38, 0] }, draw: () =>
      `<path class="lead" d="M-38 0H-22M22 0H38"/>
       <circle class="body" r="22"/>
       <g class="rotor"><path d="M0 -17V17M-17 0H17" class="blade"/><circle r="4" class="hub"/></g>
       <text y="-27">motor</text>` },
    nmos: { terms: { g: [-36, 0], d: [16, -32], s: [16, 32] }, draw: () =>
      `<path class="lead" d="M-36 0H-8M-8 -12V12M-2 -14V14M-2 -9H16V-32M-2 9H16V32M-2 0H16"/>
       <path class="lead arrow" d="M4 -4L-2 0L4 4"/>
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
  const LED_COLORS = { red: "#ff3b2f", yellow: "#ffd21a", orange: "#ff8c1a",
                       green: "#2bff55", blue: "#3b82ff", white: "#ffffff" };
  const RAIL_COLORS = { "3V3": "#ff6b5e", "5V": "#ffae42", GND: "#6f7c8c" };

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

  // Where an end is drawn, in bench coordinates
  function endPos(end) {
    if (end.startsWith("pin:") || end.startsWith("rail:")) {
      const at = data.anchors[end];
      return at ? [at[0] + 34, at[1]] : null;     // tags wire from the right
    }
    const [pid, term] = end.split(".");
    const p = data.parts.find((x) => x.id === pid);
    const off = p && SHAPES[p.type].terms[term];
    return off ? [p.x + off[0], p.y + off[1]] : null;
  }

  function netVolts(end) { return solved.nets[end]; }
  function voltColor(v) {
    if (v == null) return "#55606e";
    const t = Math.max(0, Math.min(1, v / 5));
    return `hsl(${210 - 210 * t}, ${40 + 50 * t}%, ${45 + 10 * t}%)`;
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

  // A free spot near the top left, stepping right then down
  function freeSpot() {
    const box = svg.getBoundingClientRect();
    const taken = [...data.parts.map((p) => [p.x, p.y]), ...Object.values(data.anchors)];
    for (let y = 60; y < 2000; y += 90) {
      for (let x = 170; x < Math.max(400, box.width - 60); x += 120) {
        if (!taken.some(([a, b]) => Math.abs(a - x) < 90 && Math.abs(b - y) < 70)) return [x, y];
      }
    }
    return [170, 60];
  }

  // The first free row in the tag column on the left (then a second one)
  function freeTagSpot() {
    const taken = Object.values(data.anchors);
    for (let x = 50; x < 1000; x += 90) {
      for (let y = 40; y < 1000; y += 46) {
        if (!taken.some(([a, b]) => Math.abs(a - x) < 70 && Math.abs(b - y) < 36)) return [x, y];
      }
    }
    return [50, 40];
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
    data.anchors[end] = freeTagSpot();
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

  function toBench(e) {
    const r = svg.getBoundingClientRect();
    return [e.clientX - r.left, e.clientY - r.top];
  }

  function startWire(end, e) {
    e.stopPropagation();
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
    const v = netVolts(end);
    const title = el("title");
    title.textContent = `${end}${v != null ? ` · ${fmtV(v)}` : ""}`;
    t.appendChild(title);
    t.addEventListener("pointerdown", (e) => startWire(end, e));
    g.appendChild(t);
  }

  // Drag the element's object (a part or a tag) by pointer
  function draggable(g, getPos, setPos) {
    g.addEventListener("pointerdown", (e) => {
      if (e.button !== 0 || e.target.classList.contains("term")) return;
      const [x0, y0] = toBench(e), [px, py] = getPos();
      drag = { dx: px - x0, dy: py - y0, moved: false, setPos };
      try { svg.setPointerCapture(e.pointerId); } catch (_) { /* synthetic */ }
    });
  }

  function renderAnchors() {
    for (const [end, at] of Object.entries(data.anchors)) {
      const g = el("g", { class: "tag", transform: `translate(${at[0]},${at[1]})` });
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
      terminal(g, end, 34, 0);
      layerParts.appendChild(g);
    }
  }

  function renderParts() {
    for (const p of data.parts) {
      const shape = SHAPES[p.type];
      if (!shape) continue;
      const g = el("g", { class: `part bp-${p.type}`, transform: `translate(${p.x},${p.y})` },
                   shape.draw(p));
      g.dataset.id = p.id;
      if (selected && selected.part === p.id) g.classList.add("selected");
      const st = solved.parts[p.id] || {};
      if (st.warn) g.classList.add("warn");
      const title = el("title");
      title.textContent = `${p.id}: ${lib[p.type].name}${st.warn ? ` — ${st.warn}` : ""}`;
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
        if (p.pressed === down) return;
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
        const [x] = toBench(e);
        const pos = Math.max(0, Math.min(1, (x - p.x + 20) / 40));
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

  function renderWires() {
    data.wires.forEach(([a, b], i) => {
      const pa = endPos(a), pb = endPos(b);
      if (!pa || !pb) return;
      const mx = (pa[0] + pb[0]) / 2;
      const d = `M${pa[0]} ${pa[1]}H${mx}V${pb[1]}H${pb[0]}`;
      const v = netVolts(a);
      const g = el("g", { class: "wire" });
      g.appendChild(el("path", { d, class: "wire-hit" }));
      const line = el("path", { d, class: "wire-line", stroke: voltColor(v) });
      line.dataset.end = a;
      g.appendChild(line);
      if (selected && selected.wire === i) g.classList.add("selected");
      const title = el("title");
      title.textContent = `${a} — ${b}${v != null ? ` · ${fmtV(v)}` : ""} (right-click to remove)`;
      g.appendChild(title);
      g.addEventListener("click", (e) => { e.stopPropagation(); selected = { wire: i }; render(); });
      g.addEventListener("contextmenu", (e) => {
        e.preventDefault(); selected = { wire: i }; removeSelected();
      });
      layerWires.appendChild(g);
    });
  }

  // The solved state on the drawn parts, without rebuilding them
  function paint() {
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
      if (p.type === "motor") g.dataset.rpm = st.rpm || 0;
      const lbl = g.querySelector("text");
      if (p.type === "motor" && lbl) lbl.textContent = st.rpm ? `motor ${st.rpm} rpm` : "motor";
      g.classList.toggle("warn", !!st.warn);
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
    for (const line of layerWires.querySelectorAll(".wire-line")) {
      line.setAttribute("stroke", voltColor(netVolts(line.dataset.end)));
    }
    for (const t of layerParts.querySelectorAll(".term")) {
      const v = netVolts(t.dataset.end);
      t.querySelector("title").textContent = `${t.dataset.end}${v != null ? ` · ${fmtV(v)}` : ""}`;
    }
    const warn = document.getElementById("bench-warn");
    if (warn) warn.textContent = (solved.warnings || []).join(" · ");
  }

  function render() {
    if (!svg) return;
    layerWires.textContent = "";
    layerParts.textContent = "";
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
      box.innerHTML = wiring ? `<span class="muted">wiring from <b>${wiring}</b>: click another terminal (Esc cancels)</span>`
        : `<span class="muted">click a terminal, then another, to wire them; right-click removes</span>`;
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
    pin.innerHTML = `<option value="">+ pin…</option>` +
      `<optgroup label="Rails">${["3V3", "5V", "GND"].map((r) => `<option value="rail:${r}">${r}</option>`).join("")}</optgroup>` +
      `<optgroup label="Pads">${pins.map((p) => `<option value="pin:${p.pad}">${p.pad}${p.label ? ` (${p.label})` : ""}${p.adc ? " · ADC" : ""}</option>`).join("")}</optgroup>`;
    pin.onchange = () => { if (pin.value) addAnchor(pin.value); pin.value = ""; };
    document.getElementById("bench-clear").onclick = () => {
      if (!data.parts.length && !Object.keys(data.anchors).length) return;
      if (!confirm("Clear the bench?")) return;
      data = { parts: [], wires: [], anchors: {} };
      selected = wiring = null;
      commit();
      render();
    };
  }

  function init() {
    svg = document.getElementById("bench-svg");
    layerWires = el("g");
    layerParts = el("g");
    rubber = el("path", { class: "rubber" });
    svg.append(layerWires, rubber, layerParts);
    svg.addEventListener("pointermove", (e) => {
      const [x, y] = toBench(e);
      if (drag) {
        drag.moved = true;
        drag.setPos(Math.round(x + drag.dx), Math.round(y + drag.dy));
        render();
      }
      if (wiring) {
        const p = endPos(wiring);
        if (p) rubber.setAttribute("d", `M${p[0]} ${p[1]}L${x} ${y}`);
      } else rubber.setAttribute("d", "");
    });
    svg.addEventListener("pointerup", () => {
      if (drag && drag.moved) commit();
      setTimeout(() => { drag = null; }, 0);
    });
    svg.addEventListener("click", () => {
      if (wiring || selected) { wiring = null; selected = null; render(); }
    });
    document.addEventListener("keydown", (e) => {
      if (e.target.closest && e.target.closest("input, textarea, select")) return;
      if (e.key === "Escape") { wiring = null; selected = null; render(); }
      if ((e.key === "Delete" || e.key === "Backspace") && selected) {
        e.preventDefault(); removeSelected();
      }
    });
    document.getElementById("bench-toggle").addEventListener("click", () => {
      document.getElementById("bench-pane").classList.toggle("collapsed");
    });
    requestAnimationFrame(animate);
  }

  return {
    init,
    board(msg) {
      lib = msg.bench_lib || {};
      pins = msg.bench_pins || [];
      document.getElementById("bench-pane").hidden = !Object.keys(lib).length;
      buildBar();
      render();
    },
    bench(b) {
      // Keep a part being dragged where the pointer has it, and a held
      // button or knob under the pointer
      if (drag || held) return;
      data = { parts: b.parts || [], wires: b.wires || [], anchors: b.anchors || {} };
      // A bench written by hand or by a script may leave tags unplaced
      for (const end of data.wires.flat()) {
        if (!data.anchors[end] && /^(pin|rail):/.test(end)) data.anchors[end] = freeTagSpot();
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
