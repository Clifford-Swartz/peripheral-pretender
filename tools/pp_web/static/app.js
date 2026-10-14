// Peripheral Pretender web board view. Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"use strict";

const $ = (id) => document.getElementById(id);
let ws, board, catalog = {}, adapters = {}, leds = [], clicks = {}, levels = {};

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
  $("board-name").textContent = b.name;
  $("vcom").textContent = b.vcom || "";
  $("board-img").src = `/board/${b.image}`;
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
  buildSlots();
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
// adapter's mikroBUS socket (rotated as the adapter holds it); otherwise
// the Click is centred on click_at_mm. LEDs and signal pins are overlays.
function drawClick(stack, h, id, def) {
  if (stack.dataset.click === (id || "")) return;
  stack.dataset.click = id || "";
  stack.textContent = "";
  if (!def || !def.image) return;
  const ad = h.adapter && adapters[h.adapter];
  let at = h.click_at_mm, rot = 0;
  if (ad) {
    const dx = h.at_mm[0] - ad.header_at_mm[0], dy = h.at_mm[1] - ad.header_at_mm[1];
    const img = document.createElement("img");
    img.className = "adapter-board";
    img.src = `/adapters/${ad.image}`;
    img.alt = img.title = `${ad.name} (${ad.part}) on ${h.id}`;
    place(img, [ad.origin_mm[0] + ad.size_mm[0] / 2 + dx,
                ad.origin_mm[1] - ad.size_mm[1] / 2 + dy], ad.size_mm);
    stack.appendChild(img);
    at = [ad.socket.at_mm[0] + dx, ad.socket.at_mm[1] + dy];
    rot = ad.socket.rotation;
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
  const put = (el, mm, d) => {
    el.style.left = `${((mm[0] - ox) / w) * 100}%`;
    el.style.top = `${((oy - mm[1]) / hh) * 100}%`;
    el.style.width = `${(d / w) * 100}%`;
    art.appendChild(el);
  };
  for (const led of def.leds || []) {
    const el = document.createElement("div");
    el.className = "click-led";
    el.style.background = `radial-gradient(circle, #fff 0 12%, ${led.color} 25%, ${led.color}55 48%, transparent 70%)`;
    el.title = led.id;
    put(el, led.at_mm, 3.2);
  }
  for (const sig of def.signals) {
    if (!sig.at_mm) continue;
    const el = document.createElement("div");
    el.className = "click-pin";
    el.dataset.pin = sig.pin === "int" ? h.int_pin : sig.pin;
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
        s.dataset.pin = sig.pin === "int" ? h.int_pin : sig.pin;
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
function showSignals(lv) {
  levels = lv;
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

function handle(msg) {
  switch (msg.type) {
  case "board": buildBoard(msg.board, msg.catalog, msg.adapters); break;
  case "clicks": showClicks(msg.clicks); break;
  case "levels": showSignals(msg.levels); break;
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

connect();
