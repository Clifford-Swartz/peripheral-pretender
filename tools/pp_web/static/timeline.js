// Peripheral Pretender timeline. Copyright (c) 2026 Clifford Swartz.
//
// A logic analyzer under the bench for the selected board: its pads, its
// SERCOMs' traffic decoded (UART text, SPI bytes, I2C transactions) and the
// exceptions it took, in the twin's virtual time, from the engine's event
// recorder (tools/pp/trace.py, tools/pp_web/timeline.py). Live it follows
// the twin; the wheel zooms, dragging pans back in time (and pauses),
// double-click or Live follows again.
"use strict";

const Timeline = (() => {
  const ROW = 24;
  const SPANS = [1e6, 1e7, 1e8, 1e9, 2e9, 1e10];      // ns
  const COLORS = { tx: "#4aa3ff", rx: "#ffb04a", i2c: "#b18cff", nack: "#ff6a6a",
                   spi: "#3fd0b4", irq: "#ffd166", pad: "#3ccf6e", busy: "#3ccf6e55" };
  let open = false, live = true, span = 2e9, t1 = null;
  let msg = null, pending = 0, timer = null, rowsFor = {};
  let panel, canvas, axis, labels, tip, wrap;

  const store = {
    get(k, d) { try { const v = localStorage.getItem(`pp.tl.${k}`); return v === null ? d : JSON.parse(v); } catch (_) { return d; } },
    set(k, v) { try { localStorage.setItem(`pp.tl.${k}`, JSON.stringify(v)); } catch (_) { /* private window */ } },
  };

  function init() {
    panel = $("timeline");
    canvas = $("tl-canvas");
    axis = $("tl-axis");
    labels = $("tl-labels");
    tip = $("tl-tip");
    wrap = $("tl-body");
    span = store.get("span", 2e9);
    $("tl-toggle").addEventListener("click", () => toggle());
    $("tl-close").addEventListener("click", () => toggle(false));
    $("tl-live").addEventListener("click", () => { live = true; t1 = null; request(); });
    const sel = $("tl-span");
    for (const s of SPANS) sel.add(new Option(fmtSpan(s), s));
    showSpan();
    sel.addEventListener("change", () => {
      if (sel.value === "custom") return;
      span = Number(sel.value);
      store.set("span", span);
      showSpan();
      request();
    });
    $("tl-add").addEventListener("change", (e) => { addRow(e.target.value); e.target.value = ""; });
    $("tl-rec").addEventListener("click", record);
    $("tl-replay").addEventListener("change", (e) => {
      const name = e.target.value;
      e.target.value = "";
      if (name && selected) send({ type: "replay", twin: selected, name, pace: true });
    });
    setInterval(showRecorder, 1000);
    canvas.addEventListener("wheel", wheel, { passive: false });
    canvas.addEventListener("pointerdown", drag);
    canvas.addEventListener("dblclick", () => { live = true; t1 = null; request(); });
    canvas.addEventListener("pointermove", (e) => hover(e));
    canvas.addEventListener("pointerleave", () => { tip.hidden = true; });
    new ResizeObserver(() => draw()).observe(wrap);
    if (store.get("open", false)) toggle(true);
  }

  function toggle(on) {
    open = on === undefined ? !open : on;
    panel.hidden = !open;
    $("tl-toggle").classList.toggle("on", open);
    store.set("open", open);
    clearTimeout(timer);
    if (open) poll();
  }

  // -- asking the engine -------------------------------------------------

  function request() {
    if (!open || !selected) return;
    if (pending && Date.now() - pending < 3000) return;
    pending = Date.now();
    const rows = rowsFor[selected];
    send({ type: "timeline", twin: selected, span: Math.round(span),
           t1: live ? null : Math.round(t1), suggest: true,
           width: Math.max(100, canvas.clientWidth), ...(rows ? { rows } : {}) });
  }

  function poll() {
    request();
    clearTimeout(timer);
    timer = setTimeout(poll, live ? 200 : 1500);
  }

  function data(m) {
    pending = 0;
    if (m.twin !== selected) return;
    msg = m;
    if (live && m.available) t1 = m.t1;
    $("tl-live").classList.toggle("on", live);
    const step = (m.t1 - m.t0) / 10;
    $("tl-where").textContent = m.available
      ? `${m.twin} · ${fmtTime(m.t0, step)} – ${fmtTime(m.t1, step)}${m.lost ? ` · ${m.lost} events lost` : ""}`
      : (twins[m.twin] && twins[m.twin].status.state === "running"
        ? "this QEMU build has no event recorder" : "no firmware running");
    addMenu();
    drawLabels();
    draw();
  }

  function shown() { return (msg && msg.rows) || []; }

  function keep(rows) {
    rowsFor[selected] = rows.map(({ data, ...r }) => r);
    request();
  }

  function removeRow(id) { keep(shown().filter((r) => r.id !== id)); }

  async function addRow(id) {
    if (!id || !msg) return;
    let row = (msg.suggested || []).find((r) => r.id === id);
    if (id === "pad?") {
      const pad = ((await ask("Pad to show", "Show", "PA05, PB3…")) || "").trim().toUpperCase();
      if (!/^P[A-F]\d{1,2}$/.test(pad)) return;
      row = { id: `pad:${pad}`, kind: "pad", pad, label: pad };
    }
    if (row && !shown().some((r) => r.id === row.id)) keep([...shown(), row]);
  }

  function addMenu() {
    const sel = $("tl-add");
    const have = new Set(shown().map((r) => r.id));
    const opts = (msg.suggested || []).filter((r) => !have.has(r.id));
    sel.length = 1;
    for (const r of opts) sel.add(new Option(r.label, r.id));
    sel.add(new Option("A pad by name…", "pad?"));
  }

  // -- recording and replaying -------------------------------------------------

  const recorders = {};             // twin -> its last "recorder" message

  async function record() {
    const r = recorders[selected];
    if (!selected) return;
    if (r && r.recording_since) {
      send({ type: "record", twin: selected, action: "stop" });
      return;
    }
    if (await ask("Record a run that replays exactly? The firmware restarts from reset, then every input " +
                  "(buttons, Click controls, the console) is kept with the instant it took effect.", "Record")) {
      send({ type: "record", twin: selected, action: "start" });
    }
  }

  function recorder(m) {
    recorders[m.twin] = m;
    if (m.twin === selected) showRecorder();
  }

  function showRecorder() {
    const r = recorders[selected] || {}, rec = $("tl-rec"), state = $("tl-rec-state");
    const since = r.recording_since;
    rec.textContent = since ? `⏹ Stop ${Math.round(Date.now() / 1000 - since)} s` : "⏺ Record";
    rec.classList.toggle("on", !!since);
    const sel = $("tl-replay");
    const names = (r.recordings || []).map((x) => x.name);
    if (sel.dataset.names !== names.join("\n")) {
      sel.dataset.names = names.join("\n");
      sel.length = 1;
      for (const x of r.recordings || []) {
        sel.add(new Option(`${x.name} · ${fmtDur(x.duration_ns)} · ${x.inputs} inputs`, x.name));
      }
    }
    sel.hidden = !names.length;
    const p = r.replaying;
    let text = r.error || "";
    // The verdict first, so a narrow bar still shows it; the name is in the tooltip
    if (!text && p) {
      text = p.state === "running" ? "replaying…"
        : p.state === "same" ? `✓ replayed exactly (${p.verdict.events} events)`
        : p.state === "differs" ? `✗ differs from ${fmtTime(p.verdict.differs_at, 1e7)}`
        : "the replay stalled";
    } else if (!text && since) {
      text = "recording";
    }
    state.textContent = text;
    state.title = p && !r.error ? `${p.name}: ${text}` : text;
    state.className = p && p.state === "differs" || r.error ? "bad" : p && p.state === "same" ? "good" : "";
  }

  // -- drawing ---------------------------------------------------------------

  function drawLabels() {
    labels.textContent = "";
    for (const r of shown()) {
      const d = document.createElement("div");
      d.className = `tl-label ${r.kind}`;
      d.style.height = `${ROW}px`;
      d.innerHTML = `<span></span><button title="Hide this row">×</button>`;
      d.querySelector("span").textContent = r.label;
      d.querySelector("button").addEventListener("click", () => removeRow(r.id));
      labels.appendChild(d);
    }
  }

  function setup(c, h) {
    const dpr = window.devicePixelRatio || 1, w = c.clientWidth;
    c.style.height = `${h}px`;
    if (c.width !== Math.round(w * dpr) || c.height !== Math.round(h * dpr)) {
      c.width = Math.round(w * dpr);
      c.height = Math.round(h * dpr);
    }
    const g = c.getContext("2d");
    g.setTransform(dpr, 0, 0, dpr, 0, 0);
    g.clearRect(0, 0, w, h);
    return [g, w];
  }

  function draw() {
    if (!open || !canvas) return;
    const rows = shown();
    const [g, w] = setup(canvas, Math.max(ROW, rows.length * ROW));
    const [ga] = setup(axis, 20);
    if (!msg || !msg.available) return;
    const t0 = msg.t0, tEnd = msg.t1, k = w / Math.max(1, tEnd - t0);
    const x = (t) => (t - t0) * k;
    drawAxis(ga, w, t0, tEnd, x);
    g.font = "11px ui-monospace, Consolas, monospace";
    g.textBaseline = "middle";
    rows.forEach((r, i) => {
      const y = i * ROW;
      g.fillStyle = i % 2 ? "#161b21" : "#12161b";
      g.fillRect(0, y, w, ROW);
      if (!r.data) return;
      if (r.kind === "pad") drawPad(g, r.data, y, w, x);
      else if (r.kind === "irq") drawIrq(g, r.data, y, w, x);
      else drawBus(g, r, y, w, x);
    });
  }

  function drawAxis(g, w, t0, tEnd, x) {
    const step = niceStep((tEnd - t0) / Math.max(2, w / 110));
    g.strokeStyle = "#2c333c";
    g.fillStyle = "#8a94a3";
    g.font = "10px system-ui, sans-serif";
    g.textBaseline = "middle";
    for (let t = Math.ceil(t0 / step) * step; t <= tEnd; t += step) {
      const px = Math.round(x(t)) + 0.5;
      g.beginPath(); g.moveTo(px, 12); g.lineTo(px, 20); g.stroke();
      g.fillText(fmtTime(t, step), px + 3, 7);
    }
  }

  function drawPad(g, d, y, w, x) {
    const hi = y + 5, lo = y + ROW - 5;
    for (const [a, b, duty] of d.busy) {
      const x0 = Math.max(0, x(a)), x1 = Math.min(w, Math.max(x0 + 2, x(b)));
      g.fillStyle = COLORS.busy;
      g.fillRect(x0, hi, x1 - x0, lo - hi);
      if (x1 - x0 > 34) {
        g.fillStyle = "#cfe9d6";
        g.fillText(`${Math.round(duty * 100)}%`, x0 + 4, y + ROW / 2);
      }
    }
    let level = d.level;
    g.strokeStyle = level === null ? "#555" : COLORS.pad;
    g.lineWidth = 1.5;
    g.beginPath();
    g.moveTo(0, level ? hi : lo);
    for (const [t, v] of d.edges) {
      const ex = x(t);
      g.lineTo(ex, level ? hi : lo);
      g.lineTo(ex, v ? hi : lo);
      level = v;
    }
    g.lineTo(w, level ? hi : lo);
    g.stroke();
    g.lineWidth = 1;
  }

  function drawIrq(g, d, y, w, x) {
    g.fillStyle = COLORS.irq;
    for (const [a, b] of d.spans) {
      const x0 = Math.max(0, x(a)), x1 = b === null ? w : x(b);
      g.fillRect(x0, y + 6, Math.max(1.5, x1 - x0), ROW - 12);
    }
  }

  function busColor(r, it) {
    if (it.count) return "#5c6673";
    if (r.kind === "uart") return COLORS[it.dir];
    if (r.kind === "i2c") return it.segments.some((s) => s.nack) || it.segments.some((s) => s.data_nack !== null)
      ? COLORS.nack : COLORS.i2c;
    return COLORS.spi;
  }

  function busText(r, it) {
    if (it.count) return `${it.count}×`;
    if (r.kind === "uart") return it.text;
    if (r.kind === "i2c") return it.text;
    return it.mosi.map((v) => v.toString(16).padStart(2, "0").toUpperCase()).join(" ");
  }

  // Each transfer a box; its text inside, or beside a narrow one up to the
  // next (the models move a whole transfer at one instant)
  function drawBus(g, r, y, w, x) {
    const items = r.data.items;
    items.forEach((it, i) => {
      const x0 = Math.max(0, x(it.t));
      const x1 = Math.min(w, Math.max(x0 + 3, x(it.end === null ? msg.t1 : it.end)));
      if (x1 < 0 || x0 > w) return;
      g.fillStyle = busColor(r, it);
      g.globalAlpha = 0.28;
      g.fillRect(x0, y + 3, x1 - x0, ROW - 6);
      g.globalAlpha = 1;
      g.strokeStyle = busColor(r, it);
      g.strokeRect(x0 + 0.5, y + 3.5, x1 - x0 - 1, ROW - 7);
      const text = busText(r, it).replace(/\r/g, "\\r").replace(/\n/g, "\\n");
      const inside = x1 - x0 - 6 > g.measureText(text).width || x1 - x0 > 60;
      const next = i + 1 < items.length ? Math.min(w, x(items[i + 1].t)) : w;
      const left = inside ? x0 + 4 : x1 + 3, room = (inside ? x1 - 2 : next - 3) - left;
      if (room > 14) {
        g.save();
        g.beginPath(); g.rect(left, y, room, ROW); g.clip();
        g.fillStyle = inside ? "#e6e9ee" : "#aab3bf";
        g.fillText(text, left, y + ROW / 2);
        g.restore();
      }
    });
  }

  // -- reading it --------------------------------------------------------------

  function at(e) {
    const r = canvas.getBoundingClientRect();
    if (!msg || !msg.available) return null;
    const fx = (e.clientX - r.left) / r.width;
    return { fx, t: msg.t0 + fx * (msg.t1 - msg.t0), row: shown()[Math.floor((e.clientY - r.top) / ROW)] };
  }

  function hover(e) {
    const p = at(e);
    if (!p || !p.row || !p.row.data) { tip.hidden = true; return; }
    const r = p.row, slack = (msg.t1 - msg.t0) / canvas.clientWidth * 4;
    let text = `${r.label}\n${fmtTime(p.t, 1)}`;
    if (r.kind === "pad") {
      let level = r.data.level;
      for (const [t, v] of r.data.edges) if (t <= p.t) level = v;
      const busy = r.data.busy.find(([a, b]) => a - slack <= p.t && p.t <= b + slack);
      text += busy ? `\ntoggling, ${Math.round(busy[2] * 100)}% high` : `\nlevel ${level === null ? "?" : level}`;
    } else if (r.kind === "irq") {
      const s = r.data.spans.find(([a, b]) => a - slack <= p.t && (b === null || p.t <= b + slack));
      text += s ? `\nrunning ${s[1] === null ? "now" : fmtDur(s[1] - s[0])}` : "";
      text += `\n${r.data.pends} pends in view`;
    } else {
      const it = itemAt(r, p.t);
      if (it && it.count) text += `\n${it.count} transfers (zoom in)`;
      else if (it && r.kind === "spi") {
        const hex = (a) => a.map((v) => v.toString(16).padStart(2, "0")).join(" ");
        text += `\nMOSI ${hex(it.mosi)}\nMISO ${hex(it.miso)}`;
      } else if (it) text += `\n${it.dir ? `${it.dir.toUpperCase()} ` : ""}${busText(r, it)}`;
    }
    tip.textContent = text;
    tip.hidden = false;
    // Below and right of the cursor; above it where the drawer ends
    const box = panel.getBoundingClientRect(), y = e.clientY - box.top;
    tip.style.left = `${Math.min(e.clientX - box.left + 14, box.width - 260)}px`;
    tip.style.top = `${y + 14 + tip.offsetHeight > box.height ? Math.max(0, y - 10 - tip.offsetHeight) : y + 14}px`;
  }

  // Zoom about the time under the cursor, from the view as it now stands
  // (replies lag a quick wheel); zooming in on a live view holds it there
  function wheel(e) {
    e.preventDefault();
    const p = at(e);
    if (!p) return;
    const f = Math.exp(e.deltaY * 0.0015);
    const next = Math.min(6e10, Math.max(2e3, span * f));
    if (live && f < 1) { live = false; t1 = msg.t1; }
    if (!live) {
      const snap = f < 1 ? under(p) : null;
      const tc = snap === null ? t1 - span + p.fx * span : snap;
      t1 = Math.min(msg.now, tc + (1 - p.fx) * next);
    }
    span = next;
    store.set("span", span);
    showSpan();
    pending = 0;
    request();
  }

  // A bus row's transfer at time @t: within a few pixels, or the one to
  // the left whose label (drawn beside a narrow box) the cursor is on
  function itemAt(r, t) {
    const px = (msg.t1 - msg.t0) / canvas.clientWidth;
    let left = null;
    for (const it of r.data.items) {
      const end = it.end === null ? msg.t1 : it.end;
      if (it.t - 4 * px <= t && t <= end + 4 * px) return it;
      if (end < t) left = it;
    }
    return left && t - (left.end === null ? msg.t1 : left.end) < 140 * px ? left : null;
  }

  // The time of what the cursor points at in its row (a transfer, a handler
  // run, an edge), so zooming in keeps it in view
  function under(p) {
    const r = p.row, slack = span / canvas.clientWidth * 4;
    if (!r || !r.data) return null;
    if (r.kind !== "pad" && r.kind !== "irq") {
      const it = itemAt(r, p.t);
      return it ? (it.t + (it.end === null ? msg.t1 : it.end)) / 2 : null;
    }
    let best = null, dist = slack;
    const near = (a, b) => {
      const d = p.t < a ? a - p.t : p.t > b ? p.t - b : 0;
      if (d <= dist) { dist = d; best = (a + b) / 2; }
    };
    if (r.kind === "pad") for (const [t] of r.data.edges) near(t, t);
    else for (const [a, b] of r.data.spans) near(a, b === null ? msg.t1 : b);
    return best;
  }

  // The span list shows the span, a wheel's too
  function showSpan() {
    const sel = $("tl-span");
    if (sel.options[0].value === "custom") sel.remove(0);
    if (!SPANS.includes(span)) {
      sel.add(new Option(`≈ ${fmtDur(span)}`, "custom"), 0);
      sel.value = "custom";
    } else {
      sel.value = String(span);
    }
  }

  function drag(e) {
    if (e.button !== 0 || !msg || !msg.available) return;
    const startX = e.clientX, startT1 = live ? msg.t1 : t1, w = canvas.clientWidth;
    try { canvas.setPointerCapture(e.pointerId); } catch (_) { /* synthetic */ }
    const move = (ev) => {
      const dt = (ev.clientX - startX) * span / w;
      if (Math.abs(ev.clientX - startX) < 3) return;
      live = false;
      t1 = Math.min(msg.now, Math.max(span, startT1 - dt));
      if (!pending) request();
    };
    const up = () => {
      canvas.removeEventListener("pointermove", move);
      canvas.removeEventListener("pointerup", up);
    };
    canvas.addEventListener("pointermove", move);
    canvas.addEventListener("pointerup", up);
  }

  // -- numbers -------------------------------------------------------------------

  function niceStep(raw) {
    const p = Math.pow(10, Math.floor(Math.log10(Math.max(1, raw))));
    for (const m of [1, 2, 5, 10]) if (m * p >= raw) return m * p;
    return 10 * p;
  }

  function fmtSpan(ns) {
    return ns >= 1e9 ? `${ns / 1e9} s` : ns >= 1e6 ? `${ns / 1e6} ms` : `${ns / 1e3} µs`;
  }

  function fmtDur(ns) {
    return ns >= 1e9 ? `${(ns / 1e9).toFixed(3)} s` : ns >= 1e6 ? `${(ns / 1e6).toFixed(3)} ms`
      : ns >= 1e3 ? `${(ns / 1e3).toFixed(2)} µs` : `${ns} ns`;
  }

  // Virtual time in seconds, as many decimals as @step (ns) needs
  function fmtTime(ns, step = 1e6) {
    const dec = Math.min(9, Math.max(0, Math.ceil(-Math.log10(step / 1e9))));
    return `${(ns / 1e9).toFixed(dec)} s`;
  }

  return { init, toggle, data, draw, request, recorder,
           selected: () => { msg = null; draw(); request(); showRecorder(); },
           view: () => ({ live, span, t1, t0: msg && msg.t0, shownT1: msg && msg.t1 }) };
})();
