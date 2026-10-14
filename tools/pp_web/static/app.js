// Peripheral Pretender web board view. Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"use strict";

const $ = (id) => document.getElementById(id);
let ws, board, leds = [];

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

// Board millimetres (origin bottom-left) to percentages of the image
function pos(atMm) {
  const [x0, y0] = board.origin_mm, [w, h] = board.size_mm;
  return [((atMm[0] - x0) / w) * 100, ((y0 - atMm[1]) / h) * 100];
}

function buildBoard(b) {
  board = b;
  $("board-name").textContent = b.name;
  $("board-img").src = `/board/${b.image}`;
  const layer = $("overlays");
  layer.textContent = "";
  leds = b.leds.map((led) => {
    const el = document.createElement("div");
    const [x, y] = pos(led.at_mm);
    el.className = "led";
    el.style.left = `${x}%`;
    el.style.top = `${y}%`;
    el.style.background = `radial-gradient(circle, #fff 0 12%, ${led.color} 22%, ${led.color}66 45%, transparent 70%)`;
    el.title = `${led.id} (${led.pin})`;
    layer.appendChild(el);
    return { ...led, el, group: led.pin[1].toLowerCase(), bit: +led.pin.slice(2) };
  });
  for (const btn of b.buttons) {
    const el = document.createElement("div");
    const [x, y] = pos(btn.at_mm);
    el.className = "button";
    el.style.left = `${x}%`;
    el.style.top = `${y}%`;
    el.style.width = `${(btn.size_mm[0] / b.size_mm[0]) * 100}%`;
    el.style.height = `${(btn.size_mm[1] / b.size_mm[1]) * 100}%`;
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
  case "board": buildBoard(msg.board); break;
  case "pins": showPins(msg.pins); break;
  case "uart": appendUart(msg.data); break;
  case "reset": $("console").textContent = ""; showPins({}); break;
  case "status":
    if (msg.state === "running") setStatus("running", `running ${msg.elf}`);
    else if (msg.state === "stopped") setStatus("stopped", `stopped (exit ${msg.code})`);
    else if (msg.state === "error") setStatus("error", msg.error);
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
