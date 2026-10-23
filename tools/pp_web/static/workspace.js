// Peripheral Pretender workspace. Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
//
// The workspace is a bench you can pan (drag the background) and zoom
// (wheel, or the buttons), in millimetres: the boards (app.js TwinView)
// sit where they were put, parts and wires around them. The palette on the
// left holds everything you can add: templates, boards (another board
// joins the bench), Click boards, parts and pin tags. Search it (press /),
// click an entry to add it, or drag it onto the bench: a Click onto a
// mikroBUS header or anywhere (to wire it pin by pin), a part onto a
// header (its legs plug in) or anywhere. With no board the bench shows
// templates.
"use strict";

const Workspace = (() => {
  const PX = 5;                          // screen px per mm at zoom 1
  const MIN_S = 0.15, MAX_S = 12;
  let view = { tx: 0, ty: 0, s: 1 };
  let canvas, world, svg, msg = null, fitted = false;
  let pan = null, ghost = null;
  const listeners = [];

  // -- the view -----------------------------------------------------------

  function apply() {
    world.style.transform = `translate(${view.tx}px, ${view.ty}px) scale(${view.s})`;
    const r = canvas.getBoundingClientRect(), k = PX * view.s;
    svg.setAttribute("viewBox", `${-view.tx / k} ${-view.ty / k} ${r.width / k} ${r.height / k}`);
    // The mat's grid: 10 mm squares, 2.54 mm dots
    canvas.style.backgroundSize = `${10 * k}px ${10 * k}px, ${10 * k}px ${10 * k}px`;
    canvas.style.backgroundPosition = `${view.tx}px ${view.ty}px, ${view.tx}px ${view.ty}px`;
    for (const f of listeners) f();
  }

  // Screen point to bench millimetres
  function toWorld(clientX, clientY) {
    const r = canvas.getBoundingClientRect(), k = PX * view.s;
    return [(clientX - r.left - view.tx) / k, (clientY - r.top - view.ty) / k];
  }

  function zoomAt(clientX, clientY, factor) {
    const r = canvas.getBoundingClientRect();
    const s = Math.min(MAX_S, Math.max(MIN_S, view.s * factor));
    const cx = clientX - r.left, cy = clientY - r.top;
    view.tx = cx - (cx - view.tx) * s / view.s;
    view.ty = cy - (cy - view.ty) * s / view.s;
    view.s = s;
    apply();
  }

  // A board's stage (its view in board mm, y up)
  function viewOf(b) { return b.view_mm || { origin: b.origin_mm, size: b.size_mm }; }

  // Everything on the bench: the boards' views and the parts
  function bounds() {
    const boxes = [];
    for (const t of msg ? msg.twins : []) {
      const v = viewOf(t.board);
      boxes.push([t.at[0], t.at[1] - 6, t.at[0] + v.size[0], t.at[1] + v.size[1]]);
    }
    if (typeof Bench !== "undefined") boxes.push(...Bench.boxes());
    if (!boxes.length) return null;
    return [Math.min(...boxes.map((b) => b[0])), Math.min(...boxes.map((b) => b[1])),
            Math.max(...boxes.map((b) => b[2])), Math.max(...boxes.map((b) => b[3]))];
  }

  function fit() {
    const r = canvas.getBoundingClientRect(), b = bounds();
    if (!r.width || !r.height) return;
    if (!b) { view = { tx: r.width / 2, ty: r.height / 2, s: 1 }; apply(); return; }
    const pad = 24, w = Math.max(10, b[2] - b[0]), h = Math.max(10, b[3] - b[1]);
    const s = Math.min(MAX_S, Math.max(MIN_S,
      Math.min((r.width - 2 * pad) / (w * PX), (r.height - 2 * pad) / (h * PX))));
    view = { s, tx: (r.width - w * PX * s) / 2 - b[0] * PX * s,
             ty: (r.height - h * PX * s) / 2 - b[1] * PX * s };
    apply();
  }

  // A drag on the bench's background pans it; on parts, holes, buttons and
  // controls it is theirs
  function panStart(e) {
    if (e.button === 1 || (e.button === 0 && isBackground(e.target))) {
      pan = { x: e.clientX, y: e.clientY, tx: view.tx, ty: view.ty, moved: false };
      canvas.classList.add("panning");
      if (e.button === 1) e.preventDefault();
    }
  }

  function isBackground(t) {
    return t === canvas || t === world || t === svg || t.id === "wired-layer" ||
      t.classList.contains("board-img") || t.classList.contains("overlays") ||
      t.classList.contains("board-inst") || t.classList.contains("ext-stack") ||
      (t.closest && t.closest(".click-board") && !t.closest(".click-btn, .click-stick"));
  }

  // -- the palette ----------------------------------------------------------

  // Every entry: {kind, id, name, sub, keys (searched), thumb (HTML)}
  function entries() {
    if (!msg) return [];
    const out = [];
    for (const t of msg.templates || []) {
      out.push({ kind: "template", id: t.id, name: t.name, sub: t.description,
                 keys: `${t.name} ${t.description} ${t.board}`,
                 thumb: `<img src="${boardImage(t.board)}" alt="">` });
    }
    for (const b of msg.boards || []) {
      const here = msg.twins.some((t) => t.board.id === b.id);
      out.push({ kind: "board", id: b.id, name: b.name.replace(/\s*\(.*$/, ""),
                 sub: `${(b.name.match(/\(([^)]*)\)/) || [])[1] || ""} · ${b.device || ""}${here ? " · on the bench" : ""}`,
                 keys: `${b.name} ${b.device} ${b.machine} board`,
                 thumb: `<img src="${b.image}" alt="">` });
    }
    for (const [id, c] of Object.entries(msg.catalog || {})) {
      out.push({ kind: "click", id, name: c.name, sub: `${c.chip} · ${c.bus}`,
                 keys: `${c.name} ${c.chip} ${c.bus} ${c.description} click`,
                 thumb: c.image ? `<img src="/clicks/${c.image}" alt="">` : "" });
    }
    for (const [type, d] of Object.entries(msg.bench_lib || {})) {
      if (type === "click") continue;   // Click boards have their section
      out.push({ kind: "part", id: type, name: d.name, sub: "",
                 keys: `${d.name} ${type} part`,
                 thumb: typeof Bench !== "undefined" ? Bench.icon(type) : "" });
    }
    for (const r of ["3V3", "5V", "GND"]) {
      out.push({ kind: "pin", id: `rail:${r}`, name: r, sub: "rail",
                 keys: `${r} rail power supply ground`, thumb: `<span class="pal-tag rail">${r}</span>` });
    }
    const several = msg.twins.length > 1;
    for (const p of msg.bench_pins || []) {
      const bare = p.pad.split("/").pop();
      out.push({ kind: "pin", id: `pin:${p.pad}`, name: several ? `${bare} · ${p.twin}` : bare,
                 sub: [p.label, p.adc ? "ADC" : ""].filter(Boolean).join(" · "),
                 keys: `${p.pad} ${p.label || ""} ${p.adc ? "adc analog" : ""} pad pin`,
                 thumb: `<span class="pal-tag">${p.pad}</span>` });
    }
    return out;
  }

  function boardImage(id) {
    const b = (msg.boards || []).find((x) => x.id === id);
    return b ? b.image : "";
  }

  const SECTIONS = [["template", "Templates"], ["board", "Boards"], ["click", "Click boards"],
                    ["part", "Parts"], ["pin", "Pins and rails"]];

  function renderPalette() {
    const list = document.getElementById("pal-list"), q = document.getElementById("pal-q").value
      .trim().toLowerCase().split(/\s+/).filter(Boolean);
    const all = entries().filter((e) => q.every((w) => `${e.name} ${e.sub} ${e.keys}`
      .toLowerCase().includes(w)));
    list.textContent = "";
    for (const [kind, title] of SECTIONS) {
      const items = all.filter((e) => e.kind === kind);
      if (!items.length) continue;
      const sec = document.createElement("section");
      sec.className = `pal-sec pal-${kind}`;
      const open = q.length || kind !== "pin" || !msg.twins.length;
      sec.innerHTML = `<h3>${title} <small>${items.length}</small></h3>`;
      if (!open && kind === "pin") sec.classList.add("folded");
      sec.querySelector("h3").addEventListener("click", () => sec.classList.toggle("folded"));
      const grid = document.createElement("div");
      grid.className = "pal-items";
      for (const e of items) grid.appendChild(item(e));
      sec.appendChild(grid);
      list.appendChild(sec);
    }
    if (!list.children.length) {
      list.innerHTML = `<p class="muted pal-none">Nothing matches.</p>`;
    }
  }

  function item(e) {
    const el = document.createElement("div");
    el.className = `pal-item pal-${e.kind}`;
    el.entry = e;
    el.innerHTML = `<div class="pal-thumb">${e.thumb}</div><div class="pal-text"><b></b><small></small></div>`;
    el.querySelector("b").textContent = e.name;
    el.querySelector("small").textContent = e.sub;
    el.title = `${e.name}${e.sub ? ` — ${e.sub}` : ""}\nClick to add, or drag onto the bench`;
    el.addEventListener("pointerdown", (ev) => {
      if (ev.button !== 0) return;
      ev.preventDefault();
      ghost = { e, x: ev.clientX, y: ev.clientY, el: null, moved: false };
    });
    return el;
  }

  // -- adding things -----------------------------------------------------------

  function hint(text) {
    const h = document.getElementById("ws-hint");
    h.textContent = text;
    h.hidden = !text;
    clearTimeout(hint.timer);
    if (text) hint.timer = setTimeout(() => { h.hidden = true; }, 4000);
  }

  function benchBusy() {
    return msg && msg.twins.length || (typeof Bench !== "undefined" && Bench.boxes().length);
  }

  async function useTemplate(id) {
    const t = (msg.templates || []).find((x) => x.id === id);
    if (benchBusy() && !await ask(`Set up "${t ? t.name : id}"? It replaces the boards, their Click boards and the bench.`,
                                  "Set it up")) return;
    setStatus("idle", "setting up the template");
    send({ type: "template", id });
  }

  // Another board on the bench: at @at (its top-left, bench mm) or right
  // of the others
  function useBoard(id, at) {
    setStatus("idle", "adding a board");
    send({ type: "add-board", board: id, ...(at ? { at } : {}) });
  }

  // The mikroBUS headers of every board, and where each is on the bench
  function slots() {
    if (!msg) return [];
    return msg.twins.flatMap((t) => t.board.headers.filter((h) => h.slot).map((h) => ({
      h, twin: t.key, at: boardToWorld(t, h.socket ? h.socket.at_mm : h.at_mm) })));
  }

  function boardToWorld(t, [x, y]) {
    const v = viewOf(t.board);
    return [x - v.origin[0] + t.at[0], v.origin[1] - y + t.at[1]];
  }

  function slotTaken(s) {
    const tv = typeof twins !== "undefined" && twins[s.twin];
    return !!(tv && tv.clicks[s.h.slot]);
  }

  // Plug Click @id into @s (a slot), else into a free header (the selected
  // board's first)
  function plugClick(id, s) {
    if (!msg.twins.length) { hint("Add a board first: Click boards plug into its mikroBUS headers."); return false; }
    const all = slots().sort((a, b) => (b.twin === selected) - (a.twin === selected));
    if (!all.length) return false;
    const target = s || all.find((x) => !slotTaken(x));
    if (!target) return false;
    send({ type: "click", twin: target.twin, slot: target.h.slot, click: id });
    return true;
  }

  function add(e, at) {
    switch (e.kind) {
    case "template": useTemplate(e.id); break;
    case "board": useBoard(e.id, at); break;
    case "click": {
      if (!msg.twins.length) { hint("Add a board first: a Click board runs with its MCU."); return; }
      if (at) {
        const near = slots().map((s) => [Math.hypot(s.at[0] - at[0], s.at[1] - at[1]), s])
          .sort((a, b) => a[0] - b[0])[0];
        if (near && near[0] < 30) { plugClick(e.id, near[1]); return; }
        // Anywhere else: on the bench, to wire pin by pin
        Bench.addPart("click", at, { click: e.id });
        hint("Wire its pins: click a pin, then a header hole (3V3 and GND power it).");
        return;
      }
      if (plugClick(e.id, null)) return;
      Bench.addPart("click", null, { click: e.id });
      hint("No free mikroBUS header: it is on the bench. Wire its pins to a board.");
      break;
    }
    case "part": Bench.addPart(e.id, at); break;
    case "pin": Bench.addAnchor(e.id, at); break;
    }
  }

  // Palette drags: a ghost follows the pointer; Click headers light up
  function dragMove(ev) {
    if (!ghost) return;
    if (!ghost.moved && Math.hypot(ev.clientX - ghost.x, ev.clientY - ghost.y) < 5) return;
    if (!ghost.el) {
      ghost.moved = true;
      ghost.el = document.createElement("div");
      ghost.el.className = `pal-ghost pal-${ghost.e.kind}`;
      ghost.el.innerHTML = `<div class="pal-thumb">${ghost.e.thumb}</div><span></span>`;
      ghost.el.querySelector("span").textContent = ghost.e.name;
      document.body.appendChild(ghost.el);
      if (ghost.e.kind === "click") showZones(true);
    }
    ghost.el.style.left = `${ev.clientX}px`;
    ghost.el.style.top = `${ev.clientY}px`;
    const over = overCanvas(ev);
    ghost.el.classList.toggle("over", over);
    if (ghost.e.kind === "click") {
      const at = over ? toWorld(ev.clientX, ev.clientY) : null;
      const near = at && slots().map((s) => [Math.hypot(s.at[0] - at[0], s.at[1] - at[1]), s])
        .sort((a, b) => a[0] - b[0])[0];
      for (const z of document.querySelectorAll(".slot-zone")) {
        z.classList.toggle("hot", !!near && near[0] < 30 &&
          near[1].twin === z.dataset.twin && near[1].h.slot === z.dataset.slot);
      }
    }
  }

  function dragEnd(ev) {
    if (!ghost) return;
    const g = ghost;
    ghost = null;
    showZones(false);
    if (g.el) g.el.remove();
    if (!g.moved) { add(g.e, null); return; }
    if (overCanvas(ev)) add(g.e, toWorld(ev.clientX, ev.clientY));
  }

  function overCanvas(ev) {
    const r = canvas.getBoundingClientRect();
    return ev.clientX >= r.left && ev.clientX <= r.right && ev.clientY >= r.top && ev.clientY <= r.bottom;
  }

  // Drop zones over the board's mikroBUS headers, in bench millimetres
  function showZones(on) {
    for (const z of document.querySelectorAll(".slot-zone")) z.remove();
    if (!on) return;
    for (const s of slots()) {
      const { h, at } = s;
      const z = document.createElement("div");
      z.className = "slot-zone";
      z.dataset.slot = h.slot;
      z.dataset.twin = s.twin;
      z.style.left = `${(at[0] - 12) * PX}px`;
      z.style.top = `${(at[1] - 12) * PX}px`;
      z.style.width = z.style.height = `${24 * PX}px`;
      z.innerHTML = `<span>${msg.twins.length > 1 ? `${s.twin} ` : ""}${h.id}${slotTaken(s) ? " (swap)" : ""}</span>`;
      world.appendChild(z);
    }
  }

  // -- the empty bench -----------------------------------------------------------

  function renderEmpty() {
    const box = document.getElementById("empty");
    const none = !msg.twins.length;
    box.hidden = !none;
    if (!none) return;
    const tpl = (msg.templates || []).map((t) => `
      <button class="tpl" data-id="${t.id}">
        <img src="${boardImage(t.board)}" alt="">
        <b>${t.name}</b><small>${t.description}</small>
        ${t.firmware && !t.built ? `<em>its firmware is not built here: the board's demo runs</em>` : ""}
      </button>`).join("");
    box.innerHTML = `
      <div class="empty-card">
        <h2>Start with a board</h2>
        <p>Drag one from the palette onto the bench, or pick a template. Parts,
        Click boards and pins are in the palette too: press <kbd>/</kbd> to search it.</p>
        <div class="tpls">${tpl}</div>
      </div>`;
    for (const b of box.querySelectorAll(".tpl")) {
      b.addEventListener("click", () => useTemplate(b.dataset.id));
    }
  }

  function init() {
    canvas = document.getElementById("canvas");
    world = document.getElementById("world");
    svg = document.getElementById("bench-svg");
    canvas.addEventListener("pointerdown", panStart);
    window.addEventListener("pointermove", (e) => {
      dragMove(e);
      if (!pan) return;
      const dx = e.clientX - pan.x, dy = e.clientY - pan.y;
      if (!pan.moved && Math.hypot(dx, dy) < 3) return;
      pan.moved = true;
      view.tx = pan.tx + dx;
      view.ty = pan.ty + dy;
      apply();
    });
    window.addEventListener("pointerup", (e) => {
      dragEnd(e);
      if (pan) {
        canvas.classList.remove("panning");
        // A pan is not a click on the bench (that drops the selection)
        if (pan.moved) {
          const swallow = (ev) => { ev.stopPropagation(); };
          window.addEventListener("click", swallow, { capture: true, once: true });
          setTimeout(() => window.removeEventListener("click", swallow, { capture: true }), 0);
        }
        pan = null;
      }
    });
    canvas.addEventListener("wheel", (e) => {
      e.preventDefault();
      zoomAt(e.clientX, e.clientY, Math.exp(-e.deltaY * (e.ctrlKey ? 0.01 : 0.0015)));
    }, { passive: false });
    document.getElementById("zoom-in").addEventListener("click", () => {
      const r = canvas.getBoundingClientRect();
      zoomAt(r.left + r.width / 2, r.top + r.height / 2, 1.25);
    });
    document.getElementById("zoom-out").addEventListener("click", () => {
      const r = canvas.getBoundingClientRect();
      zoomAt(r.left + r.width / 2, r.top + r.height / 2, 0.8);
    });
    document.getElementById("zoom-fit").addEventListener("click", fit);
    document.getElementById("pal-q").addEventListener("input", renderPalette);
    document.getElementById("pal-q").addEventListener("keydown", (e) => {
      if (e.key === "Escape") { e.target.value = ""; renderPalette(); e.target.blur(); }
      if (e.key === "Enter") {             // add the first match
        const first = document.querySelector(".pal-sec:not(.folded) .pal-item");
        if (first) add(first.entry, null);
      }
    });
    document.getElementById("palette-toggle").addEventListener("click", togglePalette);
    document.addEventListener("keydown", (e) => {
      if (e.target.closest && e.target.closest("input, textarea, select")) return;
      if (e.key === "/") { e.preventDefault(); showPalette(true); document.getElementById("pal-q").focus(); }
      if (e.key === "f" || e.key === "F") fit();
    });
    let open = true;
    try { open = localStorage.getItem("pp.palette") !== "closed"; } catch (_) { /* none */ }
    showPalette(open && window.innerWidth > 760);
    new ResizeObserver(() => { if (fitted) apply(); else fit(); }).observe(canvas);
    apply();
  }

  function title() {
    const t = msg && msg.twins.find((x) => x.key === selected);
    document.getElementById("ws-title").textContent = !msg || !msg.twins.length ? "empty bench"
      : `${t ? t.board.name : ""}${msg.twins.length > 1 ? ` (${msg.twins.length} boards)` : ""}`;
  }

  function showPalette(open) {
    document.body.classList.toggle("palette-closed", !open);
    try { localStorage.setItem("pp.palette", open ? "open" : "closed"); } catch (_) { /* none */ }
  }
  function togglePalette() {
    showPalette(document.body.classList.contains("palette-closed"));
  }

  return {
    init,
    PX,
    toWorld,
    fit,
    onView(f) { listeners.push(f); },
    // mm per screen px, for things drawn at a constant screen size
    unit() { return 1 / (PX * view.s); },
    board(m) {
      const sig = (x) => x ? x.twins.map((t) => `${t.key}:${t.board.id}`).join(",") : "";
      const refit = !msg || sig(msg) !== sig(m);
      msg = m;
      renderPalette();
      renderEmpty();
      title();
      if (refit) {
        // After the boards and the bench have drawn
        requestAnimationFrame(() => { fit(); fitted = true; });
      }
    },
    selected: title,
    bench() { if (msg && !msg.twins.length) renderEmpty(); },
    hint,
  };
})();
