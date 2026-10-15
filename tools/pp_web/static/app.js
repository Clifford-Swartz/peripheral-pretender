// Peripheral Pretender web board view. Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"use strict";

const $ = (id) => document.getElementById(id);
let ws, board, catalog = {}, adapters = {}, leds = [], clicks = {}, levels = {};
let pinsNow = {}, clickState = {};

// The mikroBUS pin behind a Click signal name ("int", "pwm", ...) on header h
function clickPin(h, role) {
  return (h.mikrobus && h.mikrobus[role]) || (role === "int" ? h.int_pin : role);
}

// Fraction of the last frame a pad was high: driven time from the duty
// figures, the rest at its resolved level (a pull, an external driver)
function pinHigh(pin) {
  const { group, bit } = pinRef(pin);
  const level = group in levels ? (levels[group] >>> bit) & 1 : 0;
  const g = pinsNow[group];
  if (!g) return level;
  const [hi, lo] = g[bit];
  return hi + Math.max(0, 1 - hi - lo) * level;
}

// Seven segments in a 10.6 x 16 box: a b c d e f g as polygons, dp a dot
function segPolys() {
  const h = (cy) => [[2.3, cy], [2.85, cy - .55], [7.15, cy - .55], [7.7, cy],
                     [7.15, cy + .55], [2.85, cy + .55]];
  const v = (cx, y0, y1) => [[cx, y0], [cx + .55, y0 + .55], [cx + .55, y1 - .55],
                             [cx, y1], [cx - .55, y1 - .55], [cx - .55, y0 + .55]];
  return { a: h(1.6), b: v(8.4, 2.1, 7.9), c: v(8.4, 8.1, 13.9), d: h(14.4),
           e: v(1.6, 8.1, 13.9), f: v(1.6, 2.1, 7.9), g: h(8) };
}

function connect() {
  ws = new WebSocket(`${location.protocol === "https:" ? "wss" : "ws"}://${location.host}/ws`);
  ws.onmessage = (e) => handle(JSON.parse(e.data));
  ws.onclose = () => { setStatus("error", "disconnected"); setTimeout(connect, 1000); };
}

function send(msg) {
  if (ws && ws.readyState === WebSocket.OPEN) ws.send(JSON.stringify(msg));
}

function setStatus(state, text) {
  const el = $("status");
  el.className = `status ${state}`;
  el.textContent = text;
}

// The stage: the board image plus room beside it for plugged Click boards
function view() {
  return board.view_mm || { origin: board.origin_mm, size: board.size_mm };
}

// Board millimetres (origin bottom-left) to percentages of the stage
function pos(atMm) {
  const { origin: [x0, y0], size: [w, h] } = view();
  return [((atMm[0] - x0) / w) * 100, ((y0 - atMm[1]) / h) * 100];
}

// A box of @sizeMm centred on @atMm, in stage percentages
function place(el, atMm, sizeMm) {
  const [x, y] = pos(atMm), [w, h] = view().size;
  el.style.left = `${x}%`;
  el.style.top = `${y}%`;
  el.style.width = `${(sizeMm[0] / w) * 100}%`;
  el.style.height = `${(sizeMm[1] / h) * 100}%`;
}

function pinRef(pin) {
  return { group: pin[1].toLowerCase(), bit: +pin.slice(2) };
}

function buildBoard(b, cat, ads) {
  board = b;
  catalog = cat || {};
  adapters = ads || {};
  $("board-select").value = b.id;
  $("vcom").textContent = b.vcom || "";
  $("board-img").src = `/board/${b.image}?board=${b.id}`;   // per board: no stale cache
  // The image covers the board outline; the stage may be larger
  const v = view();
  // Fit the stage in the pane at its aspect ratio (container query units)
  $("board").style.aspectRatio = `${v.size[0]} / ${v.size[1]}`;
  $("board").style.width = `min(100cqw, calc(100cqh * ${v.size[0] / v.size[1]}))`;
  place($("board-img"), [b.origin_mm[0] + b.size_mm[0] / 2,
                         b.origin_mm[1] - b.size_mm[1] / 2], b.size_mm);
  const layer = $("overlays");
  layer.textContent = "";
  leds = b.leds.map((led) => {
    const el = document.createElement("div");
    place(el, led.at_mm, [5.3, 5.3]);
    el.className = "led";
    el.style.background = `radial-gradient(circle, #fff 0 12%, ${led.color} 22%, ${led.color}66 45%, transparent 70%)`;
    el.title = `${led.id} (${led.pin})`;
    layer.appendChild(el);
    return { ...led, el, ...pinRef(led.pin) };
  });
  for (const btn of b.buttons) {
    const el = document.createElement("div");
    place(el, btn.at_mm, btn.size_mm);
    el.className = "button";
    el.innerHTML = `<span class="tag">${btn.id}</span>`;
    const press = (down) => {
      el.classList.toggle("down", down);
      send({ type: "button", id: btn.action, pressed: down });
    };
    el.addEventListener("pointerdown", (e) => {
      try { el.setPointerCapture(e.pointerId); } catch (_) { /* synthetic */ }
      press(true);
    });
    el.addEventListener("pointerup", () => press(false));
    el.addEventListener("pointercancel", () => press(false));
    layer.appendChild(el);
  }
  buildParts(b);
  buildSlots();
  // The hint names this board's buttons
  const names = b.buttons.map((x) => `<b>${x.id}</b>`);
  $("hint").innerHTML = names.length ? `Click ${names.join(" and ")} on the board` +
    ((b.parts || []).some((p) => p.sticks) ? "; drag the stick to tilt it." : ".") : "";
  $("files-head").hidden = $("files").hidden = !b.circuitpython;
}

// The board's own parts (board.json "parts"): a display panel, NeoPixels
// and an IMU to tilt, drawn like their Click counterparts; their state
// comes under "<part>.<property>" in the polled properties
function buildParts(b) {
  const layer = $("overlays");
  for (const part of b.parts || []) {
    const d = part.display;
    if (d && d.type === "panel") {
      const cv = document.createElement("canvas");
      cv.className = "click-panel";
      cv.width = d.width;
      cv.height = d.height;
      cv.dataset.prop = `${part.id}.${d.property}`;
      cv.title = part.name || part.id;
      place(cv, d.at_mm, d.size_mm);
      cv.style.transform = "translate(-50%, -50%)";
      layer.appendChild(cv);
    }
    const px = part.pixels;
    if (px) {
      px.at_mm.forEach((at, i) => {
        const el = document.createElement("div");
        el.className = "pixel";
        el.dataset.prop = `${part.id}.${px.property}`;
        el.dataset.index = i;
        el.title = `${part.name || part.id} #${i}`;
        place(el, at, [px.size_mm, px.size_mm]);
        layer.appendChild(el);
      });
    }
    const au = part.audio;
    if (au) {
      // A speaker: the DAC's waveform played in the browser
      const el = document.createElement("div");
      el.className = "click-sound board-sound";
      el.dataset.prop = `${part.id}.${au.property}`;
      el.title = `${part.name || part.id} (click to mute or unmute)`;
      el.innerHTML = "<i></i><i></i><span></span>";
      el.addEventListener("click", () => {
        muted = !muted;
        updateClickParts();
      });
      place(el, au.at_mm, [au.size_mm, au.size_mm]);
      el.style.transform = "translate(-50%, -50%)";
      layer.appendChild(el);
    }
    for (const st of part.sticks || []) {
      const el = makeStick({ part: part.id }, st, $("board-img"));
      place(el, st.at_mm, [st.size_mm, st.size_mm]);
      el.style.transform = "translate(-50%, -50%)";
      el.classList.add("board-stick");
      layer.appendChild(el);
    }
  }
}

// One row per extension header: a Click picker, then the plugged Click's
// controls (live) and signals (from the pad it drives)
function buildSlots() {
  const box = $("clicks");
  box.textContent = "";
  // Only headers the QEMU machine can plug a Click into have a slot
  const slots = board.headers.filter((x) => x.slot);
  $("clicks-head").hidden = !slots.length;
  $("clicks-where").textContent =
    `mikroBUS on ${slots.map((h) => h.id).join("/")}`;
  for (const h of slots) {
    const row = document.createElement("div");
    row.className = "slot";
    row.dataset.slot = h.slot;
    const sel = document.createElement("select");
    sel.innerHTML = `<option value="">(empty)</option>` + Object.entries(catalog)
      .map(([id, c]) => `<option value="${id}">${c.name}</option>`).join("");
    sel.addEventListener("change", () =>
      send({ type: "click", slot: h.slot, click: sel.value || null }));
    const head = document.createElement("div");
    head.className = "slot-head";
    head.innerHTML = `<b>${h.id}</b>`;
    head.appendChild(sel);
    const body = document.createElement("div");
    body.className = "slot-body";
    row.append(head, body);
    box.appendChild(row);
    const art = document.createElement("div");
    art.className = "ext-stack";
    art.dataset.slot = h.slot;
    $("overlays").appendChild(art);
  }
}

// Draw a plugged Click from its rendered top side. On a header that needs
// an adapter, the adapter's socket sits on the header and the Click on the
// adapter's mikroBUS socket (rotated as the adapter holds it); a header
// that is a mikroBUS socket itself (a Curiosity Nano Base) gives "socket"
// (where the Click's header centre goes, rotation); otherwise the Click is
// centred on click_at_mm. LEDs and signal pins are overlays.
function drawClick(stack, h, id, def) {
  if (stack.dataset.click === (id || "")) return;
  stack.dataset.click = id || "";
  stack.textContent = "";
  if (!def || !def.image) return;
  const ad = h.adapter && adapters[h.adapter];
  let at = h.click_at_mm, rot = 0, socket = h.socket, dx = 0, dy = 0;
  if (ad) {
    dx = h.at_mm[0] - ad.header_at_mm[0];
    dy = h.at_mm[1] - ad.header_at_mm[1];
    const img = document.createElement("img");
    img.className = "adapter-board";
    img.src = `/adapters/${ad.image}`;
    img.alt = img.title = `${ad.name} (${ad.part}) on ${h.id}`;
    place(img, [ad.origin_mm[0] + ad.size_mm[0] / 2 + dx,
                ad.origin_mm[1] - ad.size_mm[1] / 2 + dy], ad.size_mm);
    stack.appendChild(img);
    socket = ad.socket;
  }
  if (socket) {
    rot = socket.rotation;
    // The socket holds the Click's header centre; the image centre is
    // off it by the Click's own geometry, turned with the Click
    const hc = def.header_center_mm || [11.43, 8.89];
    const ox = def.origin_mm[0] + def.size_mm[0] / 2 - hc[0];
    const oy = def.origin_mm[1] - def.size_mm[1] / 2 - hc[1];
    const r = (rot * Math.PI) / 180;
    at = [socket.at_mm[0] + dx + ox * Math.cos(r) + oy * Math.sin(r),
          socket.at_mm[1] + dy - ox * Math.sin(r) + oy * Math.cos(r)];
  }
  const art = document.createElement("div");
  art.className = "click-board";
  stack.appendChild(art);
  const [ox, oy] = def.origin_mm, [w, hh] = def.size_mm;
  place(art, at, def.size_mm);
  art.style.transform = `translate(-50%, -50%) rotate(${rot}deg)`;
  art.title = `${def.name} on ${h.id}`;
  const img = document.createElement("img");
  img.src = `/clicks/${def.image}`;
  img.alt = def.name;
  art.appendChild(img);
  art.dataset.slot = h.slot;
  const put = (el, mm, d, dh) => {
    el.style.left = `${((mm[0] - ox) / w) * 100}%`;
    el.style.top = `${((oy - mm[1]) / hh) * 100}%`;
    el.style.width = `${(d / w) * 100}%`;
    if (dh) el.style.height = `${(dh / hh) * 100}%`;
    art.appendChild(el);
  };
  for (const led of def.leds || []) {
    const el = document.createElement("div");
    el.className = led.pin ? "click-led pinned" : "click-led";
    el.style.background = `radial-gradient(circle, #fff 0 12%, ${led.color} 25%, ${led.color}55 48%, transparent 70%)`;
    el.title = led.id;
    if (led.pin) {
      el.dataset.pin = clickPin(h, led.pin);
      el.dataset.activeLow = led.active_low ? "1" : "";
    }
    put(el, led.at_mm, led.size_mm || 3.2);
  }
  // Buttons on the Click: held while the pointer is down
  for (const btn of def.buttons || []) {
    const el = document.createElement("div");
    el.className = "click-btn";
    el.title = `${btn.id} (hold to keep it pressed)`;
    const press = (down) => {
      el.classList.toggle("down", down);
      send({ type: "control", slot: h.slot, id: btn.id, value: down ? 1 : 0 });
    };
    el.addEventListener("pointerdown", (e) => {
      try { el.setPointerCapture(e.pointerId); } catch (_) { /* synthetic */ }
      press(true);
    });
    el.addEventListener("pointerup", () => press(false));
    el.addEventListener("pointercancel", () => press(false));
    put(el, btn.at_mm, btn.size_mm[0], btn.size_mm[1]);
  }
  // Relays: their state follows the driving pin
  for (const rel of def.relays || []) {
    const el = document.createElement("div");
    el.className = "click-relay";
    el.dataset.pin = clickPin(h, rel.pin);
    el.innerHTML = `<span>${rel.id}</span><b>ON</b>`;
    el.title = `${rel.id} (${el.dataset.pin})`;
    put(el, rel.at_mm, rel.size_mm[0], rel.size_mm[1]);
  }
  // A seven-segment display: digits from a register of the Click's device
  if (def.display && def.display.type === "7seg") {
    const polys = segPolys();
    for (const d of def.display.digits) {
      const el = document.createElement("div");
      el.className = "click-7seg";
      el.dataset.shift = d.shift;
      el.dataset.prop = `${h.slot}.${def.display.property}`;
      el.dataset.enable = clickPin(h, def.display.enable_pin);
      el.style.setProperty("--seg", def.display.color);
      const segs = Object.entries(def.display.segments).map(([name, bit]) =>
        name === "dp"
          ? `<circle data-bit="${bit}" cx="9.6" cy="14.6" r=".65"/>`
          : `<polygon data-bit="${bit}" points="${polys[name].map((p) => p.join(",")).join(" ")}"/>`);
      el.innerHTML = `<svg viewBox="0 0 10.6 16" preserveAspectRatio="none">${segs.join("")}</svg>`;
      put(el, d.at_mm, d.size_mm[0], d.size_mm[1]);
    }
  }
  // An LED matrix: digit d of the display property ("d0 d1 ... d7", hex
  // bytes) lights column col_digits.indexOf(d), bit row_bits[r] row r
  if (def.display && def.display.type === "matrix") {
    const m = def.display;
    m.rows_mm.forEach((y, r) => m.cols_mm.forEach((x, c) => {
      const el = document.createElement("div");
      el.className = "click-dot";
      el.dataset.prop = `${h.slot}.${m.property}`;
      el.dataset.digit = m.col_digits[c];
      el.dataset.bit = m.row_bits[r];
      el.style.setProperty("--seg", m.color);
      put(el, [x - m.size_mm[0] / 2, y + m.size_mm[1] / 2], m.size_mm[0],
          m.size_mm[1]);
    }));
  }
  // A display panel (OLED C's SSD1351, later TFTs): the controller's
  // "frame" (base64 RGB565, rows top to bottom) on a canvas over the glass
  if (def.display && def.display.type === "panel") {
    const d = def.display, cv = document.createElement("canvas");
    cv.className = "click-panel";
    cv.width = d.width;
    cv.height = d.height;
    cv.dataset.prop = `${h.slot}.${d.property}`;
    put(cv, [d.at_mm[0] - d.size_mm[0] / 2, d.at_mm[1] + d.size_mm[1] / 2],
        d.size_mm[0], d.size_mm[1]);
  }
  // Sticks: a joystick knob to drag (springs back; hold still to push
  // it), or, in "tilt" mode, which way the Click leans (stays put;
  // double-click levels it)
  art.dataset.rot = rot;
  for (const st of def.sticks || []) {
    put(makeStick({ slot: h.slot }, st, art), [st.at_mm[0] - st.size_mm / 2,
                                     st.at_mm[1] + st.size_mm / 2],
        st.size_mm, st.size_mm);
  }
  // A text readout (RTC 6's time): a polled string property
  if (def.display && def.display.type === "text") {
    const d = def.display, el = document.createElement("div");
    el.className = "click-text";
    el.dataset.prop = `${h.slot}.${d.property}`;
    el.innerHTML = `<svg viewBox="0 0 ${d.size_mm[0]} ${d.size_mm[1]}"><text x="${d.size_mm[0] / 2}" ` +
      `y="${d.size_mm[1] * 0.72}" font-size="${d.size_mm[1] * 0.78}" fill="${d.color || "#9fe8ff"}" ` +
      `text-anchor="middle" textLength="${d.size_mm[0] * 0.92}" lengthAdjust="spacingAndGlyphs"></text></svg>`;
    put(el, [d.at_mm[0] - d.size_mm[0] / 2, d.at_mm[1] + d.size_mm[1] / 2],
        d.size_mm[0], d.size_mm[1]);
  }
  // A buzzer: its tone follows the Click's frequency property
  if (def.sound) {
    const el = document.createElement("div");
    el.className = "click-sound";
    el.dataset.slot = h.slot;
    el.dataset.prop = `${h.slot}.${def.sound.property}`;
    el.dataset.duty = `${h.slot}.${def.sound.duty_property}`;
    el.title = "Buzzer (click to mute or unmute)";
    el.innerHTML = "<i></i><i></i><span></span>";
    el.addEventListener("click", () => {
      muted = !muted;
      updateClickParts();
    });
    put(el, [def.sound.at_mm[0] - def.sound.size_mm / 2,
             def.sound.at_mm[1] + def.sound.size_mm / 2],
        def.sound.size_mm, def.sound.size_mm);
  }
  updateClickParts();
  for (const sig of def.signals) {
    if (!sig.at_mm) continue;
    const el = document.createElement("div");
    el.className = "click-pin";
    el.dataset.pin = clickPin(h, sig.pin);
    el.dataset.activeLow = sig.active_low ? "1" : "";
    el.title = `${sig.id} (${el.dataset.pin})`;
    put(el, sig.at_mm, 2.6);
  }
}

function showClicks(state) {
  clicks = state;
  for (const h of board.headers.filter((x) => x.slot)) {
    const c = state[h.slot], def = c && catalog[c.id];
    const row = document.querySelector(`.slot[data-slot="${h.slot}"]`);
    const art = document.querySelector(`.ext-stack[data-slot="${h.slot}"]`);
    const sel = row.querySelector("select"), body = row.querySelector(".slot-body");
    sel.value = c ? c.id : "";
    drawClick(art, h, c && c.id, def);
    if (!def) { body.textContent = ""; body.dataset.click = ""; continue; }
    if (body.dataset.click !== c.id) {
      body.dataset.click = c.id;
      body.innerHTML = `<div class="click-desc">${def.chip} · ${def.bus} · ${def.description}</div>`;
      for (const ctl of def.controls) {
        const r = document.createElement("label");
        r.className = "control";
        r.innerHTML = `<span>${ctl.label}</span>
          <input type="range" min="${ctl.min}" max="${ctl.max}" step="${ctl.step}">
          <output></output>`;
        const range = r.querySelector("input");
        range.dataset.id = ctl.id;
        range.addEventListener("input", () => {
          r.querySelector("output").textContent = `${(+range.value).toFixed(2)} ${ctl.unit}`;
          send({ type: "control", slot: h.slot, id: ctl.id, value: +range.value });
        });
        body.appendChild(r);
      }
      for (const sig of def.signals) {
        const s = document.createElement("div");
        s.className = "signal";
        s.dataset.pin = clickPin(h, sig.pin);
        s.dataset.activeLow = sig.active_low ? "1" : "";
        s.innerHTML = `<i></i>${sig.id} <small>(${s.dataset.pin})</small>`;
        body.appendChild(s);
      }
    }
    for (const range of body.querySelectorAll("input[type=range]")) {
      const ctl = def.controls.find((x) => x.id === range.dataset.id);
      const v = c.values[ctl.id];
      if (document.activeElement !== range) range.value = v;
      range.parentElement.querySelector("output").textContent = `${(+v).toFixed(2)} ${ctl.unit}`;
    }
  }
  showSignals(levels);
}

// A Click output is "active" while its pad sits at the active level
// Pin-driven parts on the Clicks: LEDs, relays, seven-segment digits
function updateClickParts() {
  for (const el of document.querySelectorAll(".click-led.pinned")) {
    const f = pinHigh(el.dataset.pin);
    el.style.opacity = Math.min(1, el.dataset.activeLow ? 1 - f : f);
  }
  for (const el of document.querySelectorAll(".click-relay")) {
    const on = pinHigh(el.dataset.pin) > 0.5;
    if (on !== el.classList.contains("on") && el.dataset.seen) tick();
    el.dataset.seen = "1";
    el.classList.toggle("on", on);
  }
  for (const el of document.querySelectorAll(".click-dot")) {
    const bytes = String(clickState[el.dataset.prop] || "").split(" ");
    const v = parseInt(bytes[Number(el.dataset.digit)] || "0", 16) || 0;
    el.classList.toggle("on", ((v >>> Number(el.dataset.bit)) & 1) === 1);
  }
  for (const cv of document.querySelectorAll(".click-panel")) {
    const frame = clickState[cv.dataset.prop] || "";
    if (shownFrames.get(cv) === frame) continue;
    shownFrames.set(cv, frame);
    drawPanel(cv, frame);
  }
  // NeoPixels: "RRGGBB RRGGBB ..." per chain, glowing at their colour
  for (const el of document.querySelectorAll(".pixel")) {
    const hex = String(clickState[el.dataset.prop] || "").split(" ")[Number(el.dataset.index)] || "000000";
    if (el.dataset.hex === hex) continue;
    el.dataset.hex = hex;
    const v = parseInt(hex, 16) || 0, r = v >> 16, g = (v >> 8) & 255, bl = v & 255;
    const peak = Math.max(r, g, bl), k = peak ? 255 / peak : 0;
    el.style.background = `radial-gradient(circle, #fff 0 10%, rgb(${r * k},${g * k},${bl * k}) 30%, transparent 72%)`;
    el.style.opacity = peak ? Math.min(1, 0.25 + peak / 160) : 0;
  }
  for (const el of document.querySelectorAll(".click-text")) {
    const t = el.querySelector("text"), v = String(clickState[el.dataset.prop] ?? "");
    if (t.textContent !== v) t.textContent = v;
  }
  // Speakers: each new chunk of the DAC's waveform is queued to play
  for (const el of document.querySelectorAll(".board-sound")) {
    const chunk = String(clickState[el.dataset.prop] || "");
    const seq = chunk.split(" ", 2)[1];
    if (chunk && seq !== el.dataset.seq) {
      el.dataset.seq = seq;
      el.dataset.until = String(performance.now() + 300);
      if (!muted) playPcm(chunk);
    }
    el.classList.toggle("on", performance.now() < Number(el.dataset.until || 0));
    el.classList.toggle("muted", muted);
  }
  const sounding = new Set();
  for (const el of document.querySelectorAll(".click-sound:not(.board-sound)")) {
    const hz = Number(clickState[el.dataset.prop] || 0);
    const duty = Number(clickState[el.dataset.duty] || 0) / 1000;
    const on = hz >= 20 && hz <= 20000;
    el.classList.toggle("on", on);
    el.classList.toggle("muted", muted);
    el.querySelector("span").textContent = on
      ? (hz >= 1000 ? `${(hz / 1000).toFixed(2)} kHz` : `${hz} Hz`) : "";
    if (on) sounding.add(el.dataset.slot);
    tone(el.dataset.slot, on && !muted ? hz : 0, duty);
  }
  for (const slot of Object.keys(tones)) {
    if (!sounding.has(slot) || !document.querySelector(`.click-sound[data-slot="${slot}"]`)) {
      tone(slot, 0, 0);
    }
  }
  for (const el of document.querySelectorAll(".click-7seg")) {
    const value = Number(clickState[el.dataset.prop] || 0);
    const bright = Math.min(1, pinHigh(el.dataset.enable));
    for (const seg of el.querySelectorAll("[data-bit]")) {
      const lit = (value >>> (Number(el.dataset.shift) + Number(seg.dataset.bit))) & 1;
      seg.style.opacity = lit ? Math.max(0.08, bright) : 0.07;
    }
  }
}

// A stick control on a Click (see drawClick) or a board part; @target
// is {slot} or {part}; sends {"type": "stick"}
function makeStick(target, st, art) {
  const el = document.createElement("div"), knob = document.createElement("b");
  const tilt = st.mode === "tilt";
  el.className = tilt ? "click-stick tilt" : "click-stick";
  el.title = tilt ? `${st.label || "Tilt"}: drag to lean the Click, double-click to level it`
                  : `${st.label || "Joystick"}: drag to move, hold still to push`;
  el.appendChild(knob);
  let pos = [0, 0], start = null, pressed = false, holdTimer = null, last = 0;
  const show = () => {
    knob.style.left = `${50 + pos[0] * 50}%`;
    knob.style.top = `${50 - pos[1] * 50}%`;
    if (tilt && target.part) {
      // The whole board leans; it is wide, so tilt it gently
      art.style.transform = `translate(-50%, -50%) perspective(160em) ` +
        `rotateY(${pos[0] * 12}deg) rotateX(${pos[1] * 25}deg)`;
    } else if (tilt) {
      const r = Number(art.dataset.rot || 0);
      art.style.transform = `translate(-50%, -50%) rotate(${r}deg) perspective(60em) ` +
        `rotateY(${pos[0] * 30}deg) rotateX(${pos[1] * 30}deg)`;
    }
  };
  const sendPos = (force) => {
    const now = performance.now();
    if (!force && now - last < 30) return;
    last = now;
    send({ type: "stick", ...target, id: st.id, x: pos[0], y: pos[1],
           ...(st.press_property ? { pressed } : {}) });
  };
  // Screen offset from the stick's centre to stick units (-1..1, y up),
  // turned back by the Click's rotation on the board
  const toStick = (e) => {
    const b = el.getBoundingClientRect(), rad = b.width / 2;
    const dx = e.clientX - (b.left + rad), dy = e.clientY - (b.top + rad);
    const r = (Number(art.dataset.rot || 0) * Math.PI) / 180;
    let x = (dx * Math.cos(r) + dy * Math.sin(r)) / rad;
    let y = -(-dx * Math.sin(r) + dy * Math.cos(r)) / rad;
    const m = Math.hypot(x, y);
    if (m > 1) { x /= m; y /= m; }
    return [x, y];
  };
  el.addEventListener("pointerdown", (e) => {
    try { el.setPointerCapture(e.pointerId); } catch (_) { /* synthetic */ }
    start = [e.clientX, e.clientY];
    el.classList.add("held");
    if (st.press_property) {
      holdTimer = setTimeout(() => { pressed = true; el.classList.add("pushed"); sendPos(true); }, 250);
    }
  });
  el.addEventListener("pointermove", (e) => {
    if (!start || Math.hypot(e.clientX - start[0], e.clientY - start[1]) < 3) return;
    clearTimeout(holdTimer);
    if (pressed) { pressed = false; el.classList.remove("pushed"); }
    pos = toStick(e);
    show();
    sendPos(false);
  });
  const end = () => {
    if (!start) return;
    start = null;
    clearTimeout(holdTimer);
    pressed = false;
    el.classList.remove("held", "pushed");
    if (!tilt) pos = [0, 0];                    // a joystick springs back
    show();
    sendPos(true);
  };
  el.addEventListener("pointerup", end);
  el.addEventListener("pointercancel", end);
  el.addEventListener("dblclick", () => { pos = [0, 0]; show(); sendPos(true); });
  show();
  return el;
}

// RGB565 pixels (base64, two bytes each, high first) onto a canvas
const shownFrames = new WeakMap();
function drawPanel(cv, b64) {
  const ctx = cv.getContext("2d"), img = ctx.createImageData(cv.width, cv.height);
  const n = cv.width * cv.height;
  let bytes = null;
  try { bytes = Uint8Array.from(atob(b64), (c) => c.charCodeAt(0)); } catch (_) { /* none */ }
  for (let i = 0; i < n; i++) {
    const v = bytes && bytes.length >= 2 * n ? (bytes[2 * i] << 8) | bytes[2 * i + 1] : 0;
    const r = (v >> 11) & 0x1f, g = (v >> 5) & 0x3f, b = v & 0x1f;
    img.data[4 * i] = (r << 3) | (r >> 2);
    img.data[4 * i + 1] = (g << 2) | (g >> 4);
    img.data[4 * i + 2] = (b << 3) | (b >> 2);
    img.data[4 * i + 3] = 255;
  }
  ctx.putImageData(img, 0, 0);
}

// Web Audio: browsers start it only after a user gesture on the page
let audio, muted = false;
const tones = {};
function audioCtx() {
  audio = audio || new AudioContext();
  if (audio.state === "suspended") audio.resume();
  return audio;
}
document.addEventListener("pointerdown", () => { try { audioCtx(); } catch (_) { /* no audio */ } });

// A buzzer's tone per slot: a square wave at @hz (0: silent). The level
// follows the fundamental of a pulse train of duty @duty: sin(pi * duty)
function tone(slot, hz, duty) {
  let t = tones[slot];
  if (!hz) {
    if (t) t.gain.gain.setTargetAtTime(0, t.ctx.currentTime, 0.01);
    return;
  }
  try {
    const ctx = audioCtx();
    if (!t) {
      const osc = ctx.createOscillator(), gain = ctx.createGain();
      osc.type = "square";
      gain.gain.value = 0;
      osc.connect(gain).connect(ctx.destination);
      osc.start();
      t = tones[slot] = { ctx, osc, gain };
    }
    t.osc.frequency.setTargetAtTime(hz, ctx.currentTime, 0.005);
    t.gain.gain.setTargetAtTime(0.06 * Math.sin(Math.PI * Math.min(1, duty || 0.5)),
                                ctx.currentTime, 0.01);
  } catch (_) { /* no audio */ }
}

// A chunk of a DAC's waveform, "<rate> <seq> <base64 of signed 16-bit
// little-endian samples>", queued after the previous one. The DC level
// (the DAC idles at mid-scale) is filtered out; a queue that runs more
// than half a second behind starts again.
let pcmNext = 0, pcmDc = 0;
function playPcm(chunk) {
  try {
    const ctx = audioCtx();
    const [rate, , b64] = chunk.split(" ");
    const raw = atob(b64), n = raw.length >> 1;
    if (!n) return;
    const buf = ctx.createBuffer(1, n, Number(rate)), data = buf.getChannelData(0);
    for (let i = 0; i < n; i++) {
      let v = raw.charCodeAt(2 * i) | (raw.charCodeAt(2 * i + 1) << 8);
      if (v & 0x8000) v -= 0x10000;
      pcmDc += (v - pcmDc) * 0.0005;
      data[i] = Math.max(-1, Math.min(1, (v - pcmDc) / 32768 * 0.5));
    }
    const src = ctx.createBufferSource();
    src.buffer = buf;
    src.connect(ctx.destination);
    if (pcmNext < ctx.currentTime || pcmNext > ctx.currentTime + 0.5) {
      pcmNext = ctx.currentTime + 0.08;
    }
    src.start(pcmNext);
    pcmNext += buf.duration;
  } catch (_) { /* no audio */ }
}

// A relay's click, from a short burst of noise
function tick() {
  try {
    audioCtx();
    const n = Math.floor(audio.sampleRate * 0.012);
    const buf = audio.createBuffer(1, n, audio.sampleRate), data = buf.getChannelData(0);
    for (let i = 0; i < n; i++) data[i] = (Math.random() * 2 - 1) * (1 - i / n);
    const src = audio.createBufferSource(), gain = audio.createGain();
    gain.gain.value = 0.25;
    src.buffer = buf;
    src.connect(gain).connect(audio.destination);
    src.start();
  } catch (_) { /* no audio */ }
}

function showSignals(lv) {
  levels = lv;
  updateClickParts();
  for (const s of document.querySelectorAll(".signal, .click-pin")) {
    const { group, bit } = pinRef(s.dataset.pin);
    if (!(group in lv)) { s.classList.remove("active"); continue; }
    const high = (lv[group] >>> bit) & 1;
    s.classList.toggle("active", high === (s.dataset.activeLow ? 0 : 1));
  }
}

// Brightness: the fraction of the last frame the LED's pad was driven to
// its "on" level, so a PWM-dimmed LED glows at its duty cycle
function showPins(pins) {
  pinsNow = pins;
  updateClickParts();
  for (const led of leds) {
    const g = pins[led.group];
    const f = g ? g[led.bit][led.active_low ? 1 : 0] : 0;
    // Steps (blinking) show at once; PWM levels are smoothed over frames
    led.level = Math.abs(f - (led.level ?? 0)) > 0.6 ? f
              : 0.6 * f + 0.4 * (led.level ?? 0);
    led.el.style.opacity = led.level > 0.01 ? Math.max(0.12, Math.pow(led.level, 0.6)) : 0;
    led.el.classList.toggle("on", f > 0.5);
  }
}

function appendUart(text) {
  const c = $("console");
  const stick = c.scrollTop + c.clientHeight >= c.scrollHeight - 4;
  c.textContent = (c.textContent + text.replace(/\r/g, "")).slice(-20000);
  if (stick) c.scrollTop = c.scrollHeight;
}

// The CIRCUITPY drive (CircuitPython boards): its files, and code.py to
// edit; saving restarts the twin with the new file
function showFiles(msg) {
  if (!msg.files) return;
  $("file-list").textContent = msg.files.filter((f) => !f.dir)
    .map((f) => `${f.path} (${f.size} B)`).join("\n");
  if (document.activeElement !== $("code-py")) $("code-py").value = msg.code || "";
}

function handle(msg) {
  switch (msg.type) {
  case "files": showFiles(msg); break;
  case "board":
    if (msg.boards) {
      const sel = $("board-select");
      sel.innerHTML = msg.boards.map((x) => `<option value="${x.id}">${x.name}</option>`).join("");
    }
    pinsNow = {}; levels = {}; clickState = {};
    buildBoard(msg.board, msg.catalog, msg.adapters);
    break;
  case "clicks": showClicks(msg.clicks); break;
  case "levels": showSignals(msg.levels); break;
  case "clickstate": clickState = msg.state; updateClickParts(); break;
  case "pins": showPins(msg.pins); break;
  case "uart": appendUart(msg.data); break;
  case "reset": $("console").textContent = ""; showPins({}); showSignals({}); break;
  case "status":
    $("board").classList.toggle("powered", msg.state === "running");
    if (msg.state === "running") setStatus("running", `running ${msg.elf}`);
    else if (msg.state === "stopped") setStatus("stopped", `stopped (exit ${msg.code})`);
    else if (msg.state === "error") setStatus("error", msg.error);
    else if (msg.state === "idle") setStatus("idle", msg.text);
    break;
  }
}

async function upload(file) {
  setStatus("idle", `loading ${file.name}`);
  const r = await fetch("/api/firmware", { method: "POST", body: file });
  if (!r.ok) setStatus("error", await r.text());
}

const drop = $("drop");
drop.addEventListener("dragover", (e) => { e.preventDefault(); drop.classList.add("over"); });
drop.addEventListener("dragleave", () => drop.classList.remove("over"));
drop.addEventListener("drop", (e) => {
  e.preventDefault();
  drop.classList.remove("over");
  if (e.dataTransfer.files[0]) upload(e.dataTransfer.files[0]);
});
$("file").addEventListener("change", (e) => { if (e.target.files[0]) upload(e.target.files[0]); });
$("send").addEventListener("submit", (e) => {
  e.preventDefault();
  send({ type: "uart", data: $("send-text").value + "\r\n" });
  $("send-text").value = "";
});

$("save-code").addEventListener("click", () => {
  setStatus("idle", "saving code.py, restarting");
  send({ type: "save-file", path: "code.py", text: $("code-py").value });
});

$("board-select").addEventListener("change", (e) => {
  setStatus("idle", "switching board");
  send({ type: "select-board", board: e.target.value });
});

connect();
